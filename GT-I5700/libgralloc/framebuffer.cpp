/*
* Copyright (C) 2008 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <sys/mman.h>

#include <dlfcn.h>

#include <cutils/ashmem.h>
#include <cutils/log.h>
#include <cutils/properties.h>

#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <linux/fb.h>
#include <linux/msm_mdp.h>

#include <GLES/gl.h>

#include "gralloc_priv.h"
#include "gr.h"
#include "s3c_g2d.h"

//#define GRALLOC_FB_DEBUG

#ifdef GRALLOC_FB_DEBUG
#define DEBUG_ENTER()	LOGD("Entering %s", __func__); sleep(5)
#define DEBUG_LEAVE()	LOGD("Leaving %s", __func__); sleep(5)
#else
#define DEBUG_ENTER()
#define DEBUG_LEAVE()
#endif

/*****************************************************************************/

// numbers of buffers for page flipping
#define NUM_BUFFERS 2


enum {
	PAGE_FLIP = 0x00000001,
	LOCKED = 0x00000002
};

struct fb_context_t {
	framebuffer_device_t  device;
};

/*****************************************************************************/

static void
s3c_g2d_copy_buffer(int s3c_g2d_fd, buffer_handle_t handle, unsigned long buffer_base,
		int fd, unsigned long fb_base,
		int width, int height, int format,
		int x, int y, int w, int h);

static int fb_setSwapInterval(struct framebuffer_device_t* dev,
			int interval)
{
	DEBUG_ENTER();
	fb_context_t* ctx = (fb_context_t*)dev;
	if (interval < dev->minSwapInterval || interval > dev->maxSwapInterval)
		return -EINVAL;
	// FIXME: implement fb_setSwapInterval
	DEBUG_LEAVE();
	return 0;
}

#if 0
static int fb_setUpdateRect(struct framebuffer_device_t* dev,
			int l, int t, int w, int h)
{
	if (((w|h) <= 0) || ((l|t)<0))
		return -EINVAL;

	fb_context_t* ctx = (fb_context_t*)dev;
	private_module_t* m = reinterpret_cast<private_module_t*>(
				dev->common.module);
	m->info.reserved[0] = 0x54445055; // "UPDT";
	m->info.reserved[1] = (uint16_t)l | ((uint32_t)t << 16);
	m->info.reserved[2] = (uint16_t)(l+w) | ((uint32_t)(t+h) << 16);
	return 0;
}
#endif

/* HACK ALERT */
#define FBIO_WAITFORVSYNC		_IOW ('F', 32, unsigned int)

static int fb_post(struct framebuffer_device_t* dev, buffer_handle_t buffer)
{
	DEBUG_ENTER();
	if (private_handle_t::validate(buffer) < 0)
		return -EINVAL;

	fb_context_t* ctx = (fb_context_t*)dev;

	private_handle_t const* hnd = reinterpret_cast<private_handle_t const*>(buffer);
	private_module_t* m = reinterpret_cast<private_module_t*>(
				dev->common.module);

	if (m->currentBuffer) {
		m->base.unlock(&m->base, m->currentBuffer);
		m->currentBuffer = 0;
	}

	if (hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER) {

		m->base.lock(&m->base, buffer,
			private_module_t::PRIV_USAGE_LOCKED_FOR_POST,
			0, 0, m->info.xres, m->info.yres, NULL);

		const size_t offset = hnd->base - m->framebuffer->base;
		m->info.activate = FB_ACTIVATE_VBL;
		m->info.yoffset = offset / m->finfo.line_length;
		if (ioctl(m->framebuffer->fd, FBIOPAN_DISPLAY, &m->info) == -1) {
			LOGE("FBIOPAN_DISPLAY failed");
			m->base.unlock(&m->base, buffer);
			return 0;
		}

		// wait for VSYNC
		unsigned int dummy; // No idea why is that, but it's required by the driver
		if (ioctl(m->framebuffer->fd, FBIO_WAITFORVSYNC, &dummy) < 0) {
			LOGE("FBIO_WAITFORVSYNC failed");
			return 0;
		}

		m->currentBuffer = buffer;
	} else {
		void* fb_vaddr;
		void* buffer_vaddr;

		m->base.lock(&m->base, m->framebuffer,
			GRALLOC_USAGE_SW_WRITE_RARELY,
			0, 0, m->info.xres, m->info.yres,
			&fb_vaddr);

		m->base.lock(&m->base, buffer,
			GRALLOC_USAGE_SW_READ_RARELY,
			0, 0, m->info.xres, m->info.yres,
			&buffer_vaddr);

//        memcpy(fb_vaddr, buffer_vaddr, m->finfo.line_length * m->info.yres);

		s3c_g2d_copy_buffer(m->s3c_g2d_fd, buffer, 0,
				0, m->finfo.smem_start,
				m->info.xres, m->info.yres,
				m->fbFormat, m->info.xoffset, m->info.yoffset,
				m->info.width, m->info.height);

		m->base.unlock(&m->base, buffer);
		m->base.unlock(&m->base, m->framebuffer);
	}

	DEBUG_LEAVE();
	return 0;
}

static int fb_compositionComplete(struct framebuffer_device_t* dev)
{
	DEBUG_ENTER();
	// TODO: Properly implement composition complete callback
	glFinish();

	DEBUG_LEAVE();
	return 0;
}

/*****************************************************************************/

//#define FORCE_24BPP

int mapFrameBufferLocked(struct private_module_t* module)
{
	DEBUG_ENTER();
	// already initialized...
	if (module->framebuffer) {
		return 0;
	}

	char const * const device_template[] = {
		"/dev/graphics/fb%u",
		"/dev/fb%u",
		0
	};

	int fd = -1;
	int g2d_fd = -1;
	int i=0;
	char name[64];

	while ((fd==-1) && device_template[i]) {
		snprintf(name, 64, device_template[i], 0);
		fd = open(name, O_RDWR, 0);
		i++;
	}
	if (fd < 0) {
		LOGE("Failed to open framebuffer");
		return -errno;
	}

	g2d_fd = open("/dev/s3c-g2d", O_RDWR, 0);
	if (g2d_fd < 0) {
		LOGE("Failed to open G2D device (%s)", "/dev/s3c-g2d");
		return -errno;
	}

	struct fb_fix_screeninfo finfo;
	if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
		LOGE("FBIOGET_FSCREENINFO failed");
		return -errno;
	}

	struct fb_var_screeninfo info;
	if (ioctl(fd, FBIOGET_VSCREENINFO, &info) == -1) {
		LOGE("FBIOGET_VSCREENINFO failed");
		return -errno;
	}

	info.xoffset = 0;
	info.yoffset = 0;
	info.activate = FB_ACTIVATE_NOW;

#ifdef FORCE_24BPP
	info.bits_per_pixel = 24;
#endif

	switch (info.bits_per_pixel) {
	case 32:
	case 24:
		module->fbFormat = HAL_PIXEL_FORMAT_BGRA_8888;
		break;
	case 16:
		module->fbFormat = HAL_PIXEL_FORMAT_RGB_565;
		break;
	default:
		LOGW("Unsupported pixel format (%d bpp), requesting RGB565.",
							info.bits_per_pixel);
		module->fbFormat = HAL_PIXEL_FORMAT_RGB_565;
		info.bits_per_pixel = 16;
	}

	/*
	* Request NUM_BUFFERS screens (at lest 2 for page flipping)
	*/
	info.yres_virtual = info.yres * NUM_BUFFERS;


	uint32_t flags = PAGE_FLIP;
	if (ioctl(fd, FBIOPUT_VSCREENINFO, &info) == -1) {
		info.yres_virtual = info.yres;
		flags &= ~PAGE_FLIP;
		LOGW("FBIOPUT_VSCREENINFO failed, page flipping not supported");
	}

	if (info.yres_virtual < info.yres * 2) {
		// we need at least 2 for page-flipping
		info.yres_virtual = info.yres;
		flags &= ~PAGE_FLIP;
		LOGW("page flipping not supported (yres_virtual=%d, requested=%d)",
		info.yres_virtual, info.yres*2);
	}

	if (ioctl(fd, FBIOGET_VSCREENINFO, &info) == -1)
		return -errno;

	int refreshRate = 1000000000000000LLU /
			(
				uint64_t( info.upper_margin + info.lower_margin + info.yres )
				* ( info.left_margin  + info.right_margin + info.xres )
				* info.pixclock
			);
	/* tom3q on 6.07.2010 - refresh rate fix for s3c6410 fb driver */
	refreshRate *= 100;

	if (refreshRate == 0) {
		// bleagh, bad info from the driver
		refreshRate = 60*1000;  // 60 Hz
	}

	if (int(info.width) <= 0 || int(info.height) <= 0) {
		// the driver doesn't return that information
		// default to 160 dpi
		info.width  = ((info.xres * 25.4f)/160.0f + 0.5f);
		info.height = ((info.yres * 25.4f)/160.0f + 0.5f);
	}

	float xdpi = (info.xres * 25.4f) / info.width;
	float ydpi = (info.yres * 25.4f) / info.height;
	float fps  = refreshRate / 1000.0f;

	LOGI(   "using (fd=%d)\n"
		"id           = %s\n"
		"xres         = %d px\n"
		"yres         = %d px\n"
		"xres_virtual = %d px\n"
		"yres_virtual = %d px\n"
		"bpp          = %d\n"
		"r            = %2u:%u\n"
		"g            = %2u:%u\n"
		"b            = %2u:%u\n",
		fd,
		finfo.id,
		info.xres,
		info.yres,
		info.xres_virtual,
		info.yres_virtual,
		info.bits_per_pixel,
		info.red.offset, info.red.length,
		info.green.offset, info.green.length,
		info.blue.offset, info.blue.length
	);

	LOGI(   "width        = %d mm (%f dpi)\n"
		"height       = %d mm (%f dpi)\n"
		"refresh rate = %.2f Hz\n",
		info.width,  xdpi,
		info.height, ydpi,
		fps
	);


	if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1)
		return -errno;

	if (finfo.smem_len <= 0)
		return -errno;


	module->flags = flags;
	module->info = info;
	module->finfo = finfo;
	module->xdpi = xdpi;
	module->ydpi = ydpi;
	module->fps = fps;
	module->s3c_g2d_fd = g2d_fd;

	/*
	* map the framebuffer
	*/

	int err;
	size_t fbSize = roundUpToPageSize(finfo.line_length * info.yres_virtual);
	module->framebuffer = new private_handle_t(dup(fd), fbSize,
			private_handle_t::PRIV_FLAGS_USES_PMEM);

	module->numBuffers = info.yres_virtual / info.yres;
	module->bufferMask = 0;

	void* vaddr = mmap(0, fbSize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (vaddr == MAP_FAILED) {
		LOGE("Error mapping the framebuffer (%s)", strerror(errno));
		return -errno;
	}
	module->framebuffer->base = intptr_t(vaddr);
	memset(vaddr, 0, fbSize);

	ioctl(g2d_fd, S3C_G2D_SET_BLENDING, G2D_PIXEL_ALPHA);

	DEBUG_LEAVE();
	return 0;
}

static int mapFrameBuffer(struct private_module_t* module)
{
	DEBUG_ENTER();
	pthread_mutex_lock(&module->lock);
	int err = mapFrameBufferLocked(module);
	pthread_mutex_unlock(&module->lock);
	DEBUG_LEAVE();
	return err;
}

/*****************************************************************************/

static int fb_close(struct hw_device_t *dev)
{
	DEBUG_ENTER();
	fb_context_t* ctx = (fb_context_t*)dev;
	if (ctx) {
		free(ctx);
	}
	DEBUG_LEAVE();
	return 0;
}

int fb_device_open(hw_module_t const* module, const char* name,
		hw_device_t** device)
{
	DEBUG_ENTER();
	int status = -EINVAL;
	if (!strcmp(name, GRALLOC_HARDWARE_FB0)) {
		alloc_device_t* gralloc_device;
		status = gralloc_open(module, &gralloc_device);
		if (status < 0)
			return status;

		/* initialize our state here */
		fb_context_t *dev = (fb_context_t*)malloc(sizeof(*dev));
		memset(dev, 0, sizeof(*dev));

		/* initialize the procs */
		dev->device.common.tag = HARDWARE_DEVICE_TAG;
		dev->device.common.version = 0;
		dev->device.common.module = const_cast<hw_module_t*>(module);
		dev->device.common.close = fb_close;
		dev->device.setSwapInterval = fb_setSwapInterval;
		dev->device.post            = fb_post;
		dev->device.setUpdateRect = 0;
		dev->device.compositionComplete = fb_compositionComplete;

		private_module_t* m = (private_module_t*)module;
		status = mapFrameBuffer(m);
		if (status >= 0) {
			int stride = m->finfo.line_length / (m->info.bits_per_pixel >> 3);
			const_cast<uint32_t&>(dev->device.flags) = 0;
			const_cast<uint32_t&>(dev->device.width) = m->info.xres;
			const_cast<uint32_t&>(dev->device.height) = m->info.yres;
			const_cast<int&>(dev->device.stride) = stride;
			const_cast<int&>(dev->device.format) = m->fbFormat;
			const_cast<float&>(dev->device.xdpi) = m->xdpi;
			const_cast<float&>(dev->device.ydpi) = m->ydpi;
			const_cast<float&>(dev->device.fps) = m->fps;
			const_cast<int&>(dev->device.minSwapInterval) = 1;
			const_cast<int&>(dev->device.maxSwapInterval) = 1;
#if 0
			if (m->finfo.reserved[0] == 0x5444 &&
					m->finfo.reserved[1] == 0x5055) {
				dev->device.setUpdateRect = fb_setUpdateRect;
				LOGD("UPDATE_ON_DEMAND supported");
			}
#endif
			*device = &dev->device.common;
		}
	}
	DEBUG_LEAVE();
	return status;
}

/* Copy a pmem buffer to the framebuffer */

static void
s3c_g2d_copy_buffer(int s3c_g2d_fd, buffer_handle_t handle, unsigned long buffer_base,
		int fd, unsigned long fb_base,
		int width, int height, int format,
		int x, int y, int w, int h)
{
	private_handle_t *priv = (private_handle_t*) handle;
	struct s3c_g2d_req req;
	uint32_t fmt;

	DEBUG_ENTER();

	memset(&req, 0, sizeof(req));

	switch (format) {
	case HAL_PIXEL_FORMAT_RGBA_8888:
		fmt = G2D_RGBA32;
		break;
	case HAL_PIXEL_FORMAT_RGBX_8888:
		fmt = G2D_RGBX32;
		break;
	case HAL_PIXEL_FORMAT_RGB_565:
		fmt = G2D_RGB16;
		break;
	case HAL_PIXEL_FORMAT_RGBA_5551:
		fmt = G2D_RGBA16;
		break;
	default:
		LOGE("UNSUPPORTED pixel format %d, aborting.", format);
		return;
	}

	width = (width + 7) & ~7;

	req.src.w = width;
	req.src.h = height;
	req.src.offs = 0;
	req.src.fd = priv->fd;
	req.src.base = buffer_base;
	req.src.fmt = fmt;

	req.dst.w = width;
	req.dst.h = height;
	req.dst.offs = 0;
	req.dst.fd = fd;
	req.dst.base = fb_base;
	req.dst.fmt = fmt;

	req.src.l = req.dst.l = x;
	req.src.t = req.dst.t = y;
	req.src.r = req.dst.r = x + w - 1;
	req.src.b = req.dst.b = y + h - 1;

	if (ioctl(s3c_g2d_fd, S3C_G2D_BITBLT, &req))
		LOGE("S3C_G2D_BITBLT failed = %d", -errno);

	DEBUG_LEAVE();
}
