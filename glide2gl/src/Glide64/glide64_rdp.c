/*
* Glide64 - Glide video plugin for Nintendo 64 emulators.
* Copyright (c) 2002  Dave2001
* Copyright (c) 2003-2009  Sergey 'Gonetz' Lipski
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or * any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

//****************************************************************
//
// Glide64 - Glide Plugin for Nintendo 64 emulators
// Project started on December 29th, 2001
//
// Authors:
// Dave2001, original author, founded the project in 2001, left it in 2002
// Gugaman, joined the project in 2002, left it in 2002
// Sergey 'Gonetz' Lipski, joined the project in 2002, main author since fall of 2002
// Hiroshi 'KoolSmoky' Morii, joined the project in 2007
//
//****************************************************************
//
// To modify Glide64:
// * Write your name and (optional)email, commented by your work, so I know who did it, and so that you can find which parts you modified when it comes time to send it to me.
// * Do NOT send me the whole project or file that you modified.  Take out your modified code sections, and tell me where to put them.  If people sent the whole thing, I would have many different versions, but no idea how to combine them all.
//
//****************************************************************

#include <math.h>
#include "Gfx_1.3.h"
#include "m64p.h"
#include "3dmath.h"
#include "Util.h"
#include "Combine.h"
#include "TexCache.h"
#include "FBtoScreen.h"
#include "CRC.h"
#include "GBI.h"
#include "Glide64_UCode.h"
#include "../../libretro/SDL.h"

static int cmd_ptr; /* for 64-bit elements, always <= +0x7FFF */

/* static DP_FIFO cmd_fifo; */
static DP_FIFO cmd_data[0x0003FFFF/sizeof(i64) + 1];

#ifdef __LIBRETRO__ // Prefix API
#define VIDEO_TAG(X) glide64##X

#define ReadScreen2 VIDEO_TAG(ReadScreen2)
#define PluginStartup VIDEO_TAG(PluginStartup)
#define PluginShutdown VIDEO_TAG(PluginShutdown)
#define PluginGetVersion VIDEO_TAG(PluginGetVersion)
#define CaptureScreen VIDEO_TAG(CaptureScreen)
#define ChangeWindow VIDEO_TAG(ChangeWindow)
#define CloseDLL VIDEO_TAG(CloseDLL)
#define DllTest VIDEO_TAG(DllTest)
#define DrawScreen VIDEO_TAG(DrawScreen)
#define GetDllInfo VIDEO_TAG(GetDllInfo)
#define InitiateGFX VIDEO_TAG(InitiateGFX)
#define MoveScreen VIDEO_TAG(MoveScreen)
#define RomClosed VIDEO_TAG(RomClosed)
#define RomOpen VIDEO_TAG(RomOpen)
#define ShowCFB VIDEO_TAG(ShowCFB)
#define SetRenderingCallback VIDEO_TAG(SetRenderingCallback)
#define UpdateScreen VIDEO_TAG(UpdateScreen)
#define ViStatusChanged VIDEO_TAG(ViStatusChanged)
#define ViWidthChanged VIDEO_TAG(ViWidthChanged)
#define ReadScreen VIDEO_TAG(ReadScreen)
#define FBGetFrameBufferInfo VIDEO_TAG(FBGetFrameBufferInfo)
#define FBRead VIDEO_TAG(FBRead)
#define FBWrite VIDEO_TAG(FBWrite)
#define ProcessDList VIDEO_TAG(ProcessDList)
#define ProcessRDPList VIDEO_TAG(ProcessRDPList)
#define ResizeVideoOutput VIDEO_TAG(ResizeVideoOutput)
#endif

//angrylion's macro, helps to cut overflowed values.
#define SIGN16(x) (((x) & 0x8000) ? ((x) | ~0xffff) : ((x) & 0xffff))

#ifdef __GNUC__
#define align(x) __attribute__ ((aligned(x)))
#else
#define align(x)
#endif

const char *ACmp[] = { "NONE", "THRESHOLD", "UNKNOWN", "DITHER" };

const char *Mode0[] = { "COMBINED",    "TEXEL0",
            "TEXEL1",     "PRIMITIVE",
            "SHADE",      "ENVIORNMENT",
            "1",        "NOISE",
            "0",        "0",
            "0",        "0",
            "0",        "0",
            "0",        "0" };

const char *Mode1[] = { "COMBINED",    "TEXEL0",
            "TEXEL1",     "PRIMITIVE",
            "SHADE",      "ENVIORNMENT",
            "CENTER",     "K4",
            "0",        "0",
            "0",        "0",
            "0",        "0",
            "0",        "0" };

const char *Mode2[] = { "COMBINED",    "TEXEL0",
            "TEXEL1",     "PRIMITIVE",
            "SHADE",      "ENVIORNMENT",
            "SCALE",      "COMBINED_ALPHA",
            "T0_ALPHA",     "T1_ALPHA",
            "PRIM_ALPHA",   "SHADE_ALPHA",
            "ENV_ALPHA",    "LOD_FRACTION",
            "PRIM_LODFRAC",   "K5",
            "0",        "0",
            "0",        "0",
            "0",        "0",
            "0",        "0",
            "0",        "0",
            "0",        "0",
            "0",        "0",
            "0",        "0" };

const char *Mode3[] = { "COMBINED",    "TEXEL0",
            "TEXEL1",     "PRIMITIVE",
            "SHADE",      "ENVIORNMENT",
            "1",        "0" };

const char *Alpha0[] = { "COMBINED",   "TEXEL0",
            "TEXEL1",     "PRIMITIVE",
            "SHADE",      "ENVIORNMENT",
            "1",        "0" };

#define Alpha1 Alpha0
const char *Alpha2[] = { "LOD_FRACTION", "TEXEL0",
            "TEXEL1",     "PRIMITIVE",
            "SHADE",      "ENVIORNMENT",
            "PRIM_LODFRAC",   "0" };
#define Alpha3 Alpha0

const char *FBLa[] = { "G_BL_CLR_IN", "G_BL_CLR_MEM", "G_BL_CLR_BL", "G_BL_CLR_FOG" };
const char *FBLb[] = { "G_BL_A_IN", "G_BL_A_FOG", "G_BL_A_SHADE", "G_BL_0" };
const char *FBLc[] = { "G_BL_CLR_IN", "G_BL_CLR_MEM", "G_BL_CLR_BL", "G_BL_CLR_FOG"};
const char *FBLd[] = { "G_BL_1MA", "G_BL_A_MEM", "G_BL_1", "G_BL_0" };

const char *str_zs[] = { "G_ZS_PIXEL", "G_ZS_PRIM" };

const char *str_yn[] = { "NO", "YES" };
const char *str_offon[] = { "OFF", "ON" };

const char *str_cull[] = { "DISABLE", "FRONT", "BACK", "BOTH" };

// I=intensity probably
const char *str_format[] = { "RGBA", "YUV", "CI", "IA", "I", "?", "?", "?" };
const char *str_size[]   = { "4bit", "8bit", "16bit", "32bit" };
const char *str_cm[]     = { "WRAP/NO CLAMP", "MIRROR/NO CLAMP", "WRAP/CLAMP", "MIRROR/CLAMP" };
const char *str_lod[]    = { "1", "2", "4", "8", "16", "32", "64", "128", "256", "512", "1024", "2048" };
const char *str_aspect[] = { "1x8", "1x4", "1x2", "1x1", "2x1", "4x1", "8x1" };

const char *str_filter[] = { "Point Sampled", "Average (box)", "Bilinear" };

const char *str_tlut[]   = { "TT_NONE", "TT_UNKNOWN", "TT_RGBA_16", "TT_IA_16" };

const char *str_dither[] = { "Pattern", "~Pattern", "Noise", "None" };

const char *CIStatus[]   = { "ci_main", "ci_zimg", "ci_unknown",  "ci_useless",
                            "ci_old_copy", "ci_copy", "ci_copy_self",
                            "ci_zcopy", "ci_aux", "ci_aux_copy" };

//static variables

uint32_t frame_count;  // frame counter

int ucode_error_report = true;

uint8_t microcode[4096];
uint32_t uc_crc;

//forward decls
static void CopyFrameBuffer (GrBuffer_t buffer);
static void apply_shading(VERTEX *vptr);

// ** UCODE FUNCTIONS **
#include "ucode.h"
#include "glide64_gDP.h"
#include "glide64_gSP.h"
#include "ucode00.h"
#include "ucode01.h"
#include "ucode02.h"
#include "ucode03.h"
#include "ucode04.h"
#include "ucode05.h"
#include "ucode06.h"
#include "ucode07.h"
#include "ucode08.h"
#include "ucode09.h"
#include "ucode09rdp.h"
#include "turbo3D.h"

static int reset = 0;
int old_ucode = -1;


void rdp_new(void)
{
   unsigned i, cpu;
   cpu = 0;
   rdp.vtx1 = (VERTEX*)malloc(256 * sizeof(VERTEX));
   rdp.vtx2 = (VERTEX*)malloc(256 * sizeof(VERTEX));
   rdp.vtx  = (VERTEX*)malloc(MAX_VTX * sizeof(VERTEX));
   rdp.frame_buffers = (COLOR_IMAGE*)malloc((NUMTEXBUF+2) * sizeof(COLOR_IMAGE));

   memset(rdp.vtx1, 0, 256 * sizeof(VERTEX));
   memset(rdp.vtx2, 0, 256 * sizeof(VERTEX));
   memset(rdp.vtx,  0, MAX_VTX * sizeof(VERTEX));

   rdp.vtxbuf = 0;
   rdp.vtxbuf2 = 0;
   rdp.vtx_buffer = 0;
   rdp.n_global = 0;

   for (i = 0; i < MAX_TMU; i++)
   {
      rdp.cache[i] = (CACHE_LUT*)malloc(MAX_CACHE * sizeof(CACHE_LUT));
      rdp.cur_cache[i]   = 0;
   }

   if (perf_get_cpu_features_cb)
      cpu = perf_get_cpu_features_cb();

   _gSPVertex = gSPVertex;
}

void rdp_setfuncs(void)
{
   if (settings.hacks & hack_Makers)
   {
      if (log_cb)
         log_cb(RETRO_LOG_INFO, "Applying Mischief Makers function pointer table tweak...\n");
      gfx_instruction[0][191] = uc0_tri1_mischief;
   }
}

void rdp_free(void)
{
   int i;
   if (rdp.vtx1)
      free(rdp.vtx1);
   if (rdp.vtx2)
      free(rdp.vtx2);

   for (i = 0; i < MAX_TMU; i++)
   {
      if (rdp.cache[i])
         free(rdp.cache[i]);
   }

   if (rdp.vtx)
      free(rdp.vtx);
   if (rdp.frame_buffers)
      free(rdp.frame_buffers);
}

void rdp_reset(void)
{
   int i;
   reset = 1;

   // set all vertex numbers
   for (i = 0; i < MAX_VTX; i++)
      rdp.vtx[i].number = i;

   rdp.scissor_o.ul_x = 0;
   rdp.scissor_o.ul_y = 0;
   rdp.scissor_o.lr_x = 320;
   rdp.scissor_o.lr_y = 240;

   rdp.vi_org_reg = *gfx_info.VI_ORIGIN_REG;
   rdp.view_scale[2] = 32.0f * 511.0f;
   rdp.view_trans[2] = 32.0f * 511.0f;
   rdp.clip_ratio = 1.0f;

   rdp.lookat[0][0] = rdp.lookat[1][1] = 1.0f;

   rdp.allow_combine = 1;
   rdp.update = UPDATE_SCISSOR | UPDATE_COMBINE | UPDATE_ZBUF_ENABLED | UPDATE_CULL_MODE;
   rdp.fog_mode = FOG_MODE_ENABLED;
   rdp.maincimg[0].addr = rdp.maincimg[1].addr = rdp.last_drawn_ci_addr = 0x7FFFFFFF;
}


static uint32_t d_ul_x, d_ul_y, d_lr_x, d_lr_y;

static void DrawPartFrameBufferToScreen(void)
{
   FB_TO_SCREEN_INFO fb_info;
   fb_info.addr   = rdp.cimg;
   fb_info.size   = rdp.ci_size;
   fb_info.width  = rdp.ci_width;
   fb_info.height = rdp.ci_height;
   fb_info.ul_x = d_ul_x;
   fb_info.lr_x = d_lr_x;
   fb_info.ul_y = d_ul_y;
   fb_info.lr_y = d_lr_y;
   fb_info.opaque = 0;
   DrawFrameBufferToScreen(&fb_info);
   memset(gfx_info.RDRAM+rdp.cimg, 0, (rdp.ci_width*rdp.ci_height)<<rdp.ci_size>>1);
}

#define RGBA16TO32(color) \
  ((color&1)?0xFF:0) | \
  ((uint32_t)(((color & 0xF800) >> 11)) << 24) | \
  ((uint32_t)(((color & 0x07C0) >> 6)) << 16) | \
  ((uint32_t)(((color & 0x003E) >> 1)) << 8)

extern uint16_t *frameBuffer;

static void CopyFrameBuffer (GrBuffer_t buffer)
{
   // don't bother to write the stuff in asm... the slow part is the read from video card,
   //   not the copy.

   uint32_t width, height;
   width = rdp.ci_width;//*gfx_info.VI_WIDTH_REG;

   FRDP ("CopyFrameBuffer: %08lx... ", rdp.cimg);

   if (fb_emulation_enabled)
   {
      int ind = (rdp.ci_count > 0)?rdp.ci_count-1:0;
      height = rdp.frame_buffers[ind].height;
   }
   else
   {
      height = rdp.ci_lower_bound;
   }
   FRDP ("width: %d, height: %d...  ", width, height);

   if (rdp.scale_x < 1.1f)
   {
      uint16_t * ptr_src = (uint16_t*)frameBuffer;
      if (grLfbReadRegion(buffer,
               (uint32_t)rdp.offset_x,
               (uint32_t)rdp.offset_y,//rdp.ci_upper_bound,
               width,
               height,
               width<<1,
               ptr_src))
      {
         uint16_t *ptr_dst = (uint16_t*)(gfx_info.RDRAM+rdp.cimg);
         uint32_t *ptr_dst32 = (uint32_t*)(gfx_info.RDRAM+rdp.cimg);
         uint16_t c;
         uint32_t y, x;

         for (y = 0; y < height; y++)
         {
            for (x = 0; x < width; x++)
            {
               c = ptr_src[x + y * width];
               if ((settings.frame_buffer & fb_read_alpha) && c <= 0) {}
               else
                  c = (c&0xFFC0) | ((c&0x001F) << 1) | 1;
               if (rdp.ci_size == 2)
                  ptr_dst[(x + y * width)^1] = c;
               else
                  ptr_dst32[x + y * width] = RGBA16TO32(c);
            }
         }
      }
   }
   else
   {
      {
         GrLfbInfo_t info;
         float scale_x = (settings.scr_res_x - rdp.offset_x*2.0f)  / max(width, rdp.vi_width);
         float scale_y = (settings.scr_res_y - rdp.offset_y*2.0f) / max(height, rdp.vi_height);

         FRDP("width: %d, height: %d, ul_y: %d, lr_y: %d, scale_x: %f, scale_y: %f, ci_width: %d, ci_height: %d\n",width, height, rdp.ci_upper_bound, rdp.ci_lower_bound, scale_x, scale_y, rdp.ci_width, rdp.ci_height);
         info.size = sizeof(GrLfbInfo_t);

         if (grLfbLock (GR_LFB_READ_ONLY,
                  buffer,
                  GR_LFBWRITEMODE_565,
                  GR_ORIGIN_UPPER_LEFT,
                  FXFALSE,
                  &info))
         {
            int y, x, x_start, y_start, x_end, y_end, read_alpha;
            uint16_t c;
            uint16_t *ptr_src = (uint16_t*)info.lfbPtr;
            uint16_t *ptr_dst = (uint16_t*)(gfx_info.RDRAM+rdp.cimg);
            uint32_t *ptr_dst32 = (uint32_t*)(gfx_info.RDRAM+rdp.cimg);
            uint32_t stride = info.strideInBytes>>1;

            read_alpha = settings.frame_buffer & fb_read_alpha;
            if ((settings.hacks&hack_PMario) && rdp.frame_buffers[rdp.ci_count-1].status != CI_AUX)
               read_alpha = false;
            x_start = 0;
			y_start = 0;
			x_end = width;
			y_end = height;

            if (settings.hacks&hack_BAR)
               x_start = 80, y_start = 24, x_end = 240, y_end = 86;

            for (y = y_start; y < y_end; y++)
            {
               for (x = x_start; x < x_end; x++)
               {
                  c = ptr_src[(int)(x*scale_x + rdp.offset_x) + (int)(y * scale_y + rdp.offset_y) * stride];
                  c = (c&0xFFC0) | ((c&0x001F) << 1) | 1;
                  if (read_alpha && c == 1)
                     c = 0;
                  if (rdp.ci_size <= 2)
                     ptr_dst[(x + y * width)^1] = c;
                  else
                     ptr_dst32[x + y * width] = RGBA16TO32(c);
               }
            }

            // Unlock the backbuffer
            grLfbUnlock (GR_LFB_READ_ONLY, buffer);
            LRDP("LfbLock.  Framebuffer copy complete.\n");
         }
         else
         {
            LRDP("Framebuffer copy failed.\n");
         }
      }
   }
}

/******************************************************************
Function: ProcessDList
Purpose:  This function is called when there is a Dlist to be
processed. (High level GFX list)
input:    none
output:   none
*******************************************************************/
void DetectFrameBufferUsage(void);
uint32_t fbreads_front = 0;
uint32_t fbreads_back = 0;
int cpu_fb_read_called = false;
int cpu_fb_write_called = false;
int cpu_fb_write = false;
int cpu_fb_ignore = false;
int CI_SET = true;
uint32_t swapped_addr = 0;
uint32_t ucode5_texshiftaddr = 0;
uint32_t ucode5_texshiftcount = 0;
uint16_t ucode5_texshift = 0;
int depth_buffer_fog;

EXPORT void CALL ProcessDList(void)
{
  uint32_t dlist_start, dlist_length, a;

  no_dlist = false;
  update_screen_count = 0;
  ChangeSize ();

  if (reset)
  {
    reset = 0;
    if (settings.autodetect_ucode)
    {
      // Thanks to ZeZu for ucode autodetection!!!
      uint32_t startUcode = *(uint32_t*)(gfx_info.DMEM+0xFD0);
      memcpy (microcode, gfx_info.RDRAM+startUcode, 4096);
      microcheck ();
    }
    else
      memset (microcode, 0, 4096);
  }
  else if ( ((old_ucode == ucode_S2DEX) && (settings.ucode == ucode_F3DEX)) || settings.force_microcheck)
  {
    uint32_t startUcode = *(uint32_t*)(gfx_info.DMEM+0xFD0);
    memcpy (microcode, gfx_info.RDRAM+startUcode, 4096);
    microcheck ();
  }

  if (exception)
    return;

  //* Set states *//
  if (settings.swapmode > 0)
    SwapOK = true;
  rdp.updatescreen = 1;

  rdp.model_i = 0; // 0 matrices so far in stack
  //stack_size can be less then 32! Important for Silicon Vally. Thanks Orkin!
  rdp.model_stack_size = min(32, (*(uint32_t*)(gfx_info.DMEM+0x0FE4))>>6);
  if (rdp.model_stack_size == 0)
    rdp.model_stack_size = 32;
  rdp.fb_drawn = rdp.fb_drawn_front = false;
  rdp.update = 0x7FFFFFFF;  // All but clear cache
  rdp.geom_mode = 0;
  rdp.maincimg[1] = rdp.maincimg[0];
  rdp.skip_drawing = false;
  rdp.s2dex_tex_loaded = false;
  rdp.bg_image_height = 0xFFFF;
  fbreads_front = fbreads_back = 0;
  rdp.fog_multiplier = rdp.fog_offset = 0;
  rdp.zsrc = 0;
  if (rdp.vi_org_reg != *gfx_info.VI_ORIGIN_REG)
    rdp.tlut_mode = 0; //is it correct?
  rdp.scissor_set = false;
  ucode5_texshiftaddr = ucode5_texshiftcount = 0;
  cpu_fb_write = false;
  cpu_fb_read_called = false;
  cpu_fb_write_called = false;
  cpu_fb_ignore = false;
  d_ul_x = 0xffff;
  d_ul_y = 0xffff;
  d_lr_x = 0;
  d_lr_y = 0;
  depth_buffer_fog = true;

  //analyze possible frame buffer usage
  if (fb_emulation_enabled)
    DetectFrameBufferUsage();
  if (!(settings.hacks&hack_Lego) || rdp.num_of_ci > 1)
    rdp.last_bg = 0;
  //* End of set states *//

  // Get the start of the display list and the length of it
  dlist_start = *(uint32_t*)(gfx_info.DMEM+0xFF0);
  dlist_length = *(uint32_t*)(gfx_info.DMEM+0xFF4);
  FRDP("--- NEW DLIST --- crc: %08lx, ucode: %d, fbuf: %08lx, fbuf_width: %d, dlist start: %08lx, dlist_length: %d, x_scale: %f, y_scale: %f\n", uc_crc, settings.ucode, *gfx_info.VI_ORIGIN_REG, *gfx_info.VI_WIDTH_REG, dlist_start, dlist_length, (*gfx_info.VI_X_SCALE_REG & 0xFFF)/1024.0f, (*gfx_info.VI_Y_SCALE_REG & 0xFFF)/1024.0f);
  FRDP_E("--- NEW DLIST --- crc: %08lx, ucode: %d, fbuf: %08lx\n", uc_crc, settings.ucode, *gfx_info.VI_ORIGIN_REG);

  // Do nothing if dlist is empty
  if (dlist_start == 0)
     return;

  if (cpu_fb_write == true)
    DrawPartFrameBufferToScreen();
  if ((settings.hacks&hack_Tonic) && dlist_length < 16)
  {
    rdp_fullsync(rdp.cmd0, rdp.cmd1);
    FRDP_E("DLIST is too short!\n");
    return;
  }

  // Start executing at the start of the display list
  rdp.pc_i = 0;
  rdp.pc[rdp.pc_i] = dlist_start;
  rdp.dl_count = -1;
  rdp.halt = 0;

  if (settings.ucode == ucode_Turbo3d)
     Turbo3D();
  else
  {
     // MAIN PROCESSING LOOP
     do {
        // Get the address of the next command
        a = rdp.pc[rdp.pc_i] & BMASK;

        // Load the next command and its input
        rdp.cmd0 = ((uint32_t*)gfx_info.RDRAM)[a>>2];   // \ Current command, 64 bit
        rdp.cmd1 = ((uint32_t*)gfx_info.RDRAM)[(a>>2)+1]; // /
        // cmd2 and cmd3 are filled only when needed, by the function that needs them

#ifdef LOG_COMMANDS
        // Output the address before the command
        FRDP ("%08lx (c0:%08lx, c1:%08lx): ", a, rdp.cmd0, rdp.cmd1);
#endif

        // Go to the next instruction
        rdp.pc[rdp.pc_i] = (a+8) & BMASK;

        // Process this instruction
        gfx_instruction[settings.ucode][rdp.cmd0>>24](rdp.cmd0, rdp.cmd1);

        // check DL counter
        if (rdp.dl_count != -1)
        {
           rdp.dl_count --;
           if (rdp.dl_count == 0)
           {
              rdp.dl_count = -1;

              LRDP("End of DL\n");
              rdp.pc_i --;
           }
        }
     } while (!rdp.halt);
  }

  if (fb_emulation_enabled)
  {
    rdp.scale_x = rdp.scale_x_bak;
    rdp.scale_y = rdp.scale_y_bak;
  }
  if (settings.frame_buffer & fb_ref)
    CopyFrameBuffer (GR_BUFFER_BACKBUFFER);

  if ((settings.hacks&hack_TGR2) && rdp.vi_org_reg != *gfx_info.VI_ORIGIN_REG && CI_SET)
  {
    newSwapBuffers ();
    CI_SET = false;
  }
  LRDP("ProcessDList end\n");
}

// undef - undefined instruction, always ignore
static void undef(uint32_t w0, uint32_t w1)
{
#ifdef _ENDUSER_RELEASE_
  *gfx_info.MI_INTR_REG |= 0x20;
  gfx_info.CheckInterrupts();
  rdp.halt = 1;
#else
  FRDP_E("** undefined ** (%08lx)\n", w0);
  FRDP("** undefined ** (%08lx) - IGNORED\n", w0);
#endif
}

// spnoop - no operation, always ignore
static void spnoop(uint32_t w0, uint32_t w1)
{
  LRDP("spnoop\n");
}

// noop - no operation, always ignore
static void rdp_noop(uint32_t w0, uint32_t w1)
{
  LRDP("noop\n");
}

static void ys_memrect(uint32_t w0, uint32_t w1)
{
  uint32_t off_x, off_y, tile, lr_x, lr_y, ul_x, ul_y;
  uint32_t y, width, tex_width;
  uint8_t *texaddr, *fbaddr;

  tile = (uint16_t)((w1 & 0x07000000) >> 24);

  lr_x = (uint16_t)((w0 & 0x00FFF000) >> 14);
  lr_y = (uint16_t)((w0 & 0x00000FFF) >> 2);
  ul_x = (uint16_t)((w1 & 0x00FFF000) >> 14);
  ul_y = (uint16_t)((w1 & 0x00000FFF) >> 2);

  if (lr_y > rdp.scissor_o.lr_y)
    lr_y = rdp.scissor_o.lr_y;

  off_x = ((rdp.cmd2 & 0xFFFF0000) >> 16) >> 5;
  off_y = (rdp.cmd2 & 0x0000FFFF) >> 5;

#ifndef NDEBUG
  FRDP ("memrect (%d, %d, %d, %d), ci_width: %d", ul_x, ul_y, lr_x, lr_y, rdp.ci_width);
  if (off_x > 0)
    FRDP ("  off_x: %d", off_x);
  if (off_y > 0)
    FRDP ("  off_y: %d", off_y);
  LRDP("\n");
#endif

  width = lr_x - ul_x;
  tex_width = rdp.tiles[tile].line << 3;
  texaddr = (uint8_t*)(gfx_info.RDRAM + rdp.addr[rdp.tiles[tile].t_mem] + tex_width*off_y + off_x);
  fbaddr = (uint8_t*)(gfx_info.RDRAM + rdp.cimg + ul_x);

  for (y = ul_y; y < lr_y; y++) {
    uint8_t *src, *dst;
    src = (uint8_t*)(texaddr + (y - ul_y) * tex_width);
    dst = (uint8_t*)(fbaddr + y * rdp.ci_width);
    memcpy (dst, src, width);
  }
}

static void pm_palette_mod(void)
{
   uint8_t envr, envg, envb;
   uint16_t env16, prmr, prmg, prmb, prim16, *dst;
   int8_t i;

   envr = (uint8_t)(rdp.env_color_sep[0] * 0.0039215689f * 31.0f);
   envg = (uint8_t)(rdp.env_color_sep[1] * 0.0039215689f * 31.0f);
   envb = (uint8_t)(rdp.env_color_sep[2] * 0.0039215689f * 31.0f);
   env16 = (uint16_t)((envr<<11)|(envg<<6)|(envb<<1)|1);
   prmr = (uint8_t)(rdp.prim_color_sep[0] * 0.0039215689f * 31.0f);
   prmg = (uint8_t)(rdp.prim_color_sep[1] * 0.0039215689f * 31.0f);
   prmb = (uint8_t)(rdp.prim_color_sep[2] * 0.0039215689f * 31.0f);
   prim16 = (uint16_t)((prmr << 11)|(prmg << 6)|(prmb << 1)|1);
   dst = (uint16_t*)(gfx_info.RDRAM+rdp.cimg);

   for (i = 0; i < 16; i++)
      dst[i^1] = (rdp.pal_8[i]&1) ? prim16 : env16;

   //LRDP("Texrect palette modification\n");
}

static void pd_zcopy(uint32_t w0, uint32_t w1)
{
   int x;
   uint16_t ul_x = (uint16_t)((w1 & 0x00FFF000) >> 14);
   uint16_t lr_x = (uint16_t)((w0 & 0x00FFF000) >> 14) + 1;
   uint16_t ul_u = (uint16_t)((rdp.cmd2 & 0xFFFF0000) >> 21) + 1;
   uint16_t *ptr_dst = (uint16_t*)(gfx_info.RDRAM+rdp.cimg);
   uint16_t width = lr_x - ul_x;
   uint16_t * ptr_src = ((uint16_t*)rdp.tmem)+ul_u;
   uint16_t c;
   for (x = 0; x < width; x++)
   {
      c = ptr_src[x];
      c = ((c<<8)&0xFF00) | (c >> 8);
      ptr_dst[(ul_x+x)^1] = c;
      //      FRDP("dst[%d]=%04lx \n", (x + ul_x)^1, c);
   }
}

static void DrawDepthBufferFog(void)
{
  FB_TO_SCREEN_INFO fb_info;
  if (rdp.zi_width < 200)
    return;

  fb_info.addr   = rdp.zimg;
  fb_info.size   = 2;
  fb_info.width  = rdp.zi_width;
  fb_info.height = rdp.ci_height;
  fb_info.ul_x = rdp.scissor_o.ul_x;
  fb_info.lr_x = rdp.scissor_o.lr_x;
  fb_info.ul_y = rdp.scissor_o.ul_y;
  fb_info.lr_y = rdp.scissor_o.lr_y;
  fb_info.opaque = 0;

  DrawDepthBufferToScreen(&fb_info);
}

static void apply_shading(VERTEX *vptr)
{
   int i;
   for (i = 0; i < 4; i++)
   {
      vptr[i].shade_mod = 0;
      apply_shade_mods (&vptr[i]);
   }
}

static void rdp_texrect(uint32_t w0, uint32_t w1)
{
   float ul_x, ul_y, lr_x, lr_y;
   uint32_t a;
   uint8_t cmdHalf1, cmdHalf2;
   int i;
   int32_t off_x_i, off_y_i;
   uint32_t tile, prev_tile;
   float Z, dsdx, dtdy, s_ul_x, s_lr_x, s_ul_y, s_lr_y, off_size_x, off_size_y;
   struct {
      float ul_u, ul_v, lr_u, lr_v;
   } texUV[2]; //struct for texture coordinates
   VERTEX *vptr = NULL, vstd[4];

   if (!rdp.LLE)
   {
      a = rdp.pc[rdp.pc_i];
      cmdHalf1 = gfx_info.RDRAM[a+3];
      cmdHalf2 = gfx_info.RDRAM[a+11];
      a >>= 2;

      if ((cmdHalf1 == 0xE1 && cmdHalf2 == 0xF1) || (cmdHalf1 == 0xB4 && cmdHalf2 == 0xB3) || (cmdHalf1 == 0xB3 && cmdHalf2 == 0xB2))
      {
         //gSPTextureRectangle
         rdp.cmd2 = ((uint32_t*)gfx_info.RDRAM)[a+1];
         rdp.cmd3 = ((uint32_t*)gfx_info.RDRAM)[a+3];
         rdp.pc[rdp.pc_i] += 16;
      }
      else
      {
         //gDPTextureRectangle
         if (settings.hacks&hack_ASB)
            rdp.cmd2 = 0;
         else
            rdp.cmd2 = ((uint32_t*)gfx_info.RDRAM)[a+0];
         rdp.cmd3 = ((uint32_t*)gfx_info.RDRAM)[a+1];
         rdp.pc[rdp.pc_i] += 8;
      }
   }
   if ((settings.hacks&hack_Yoshi) && settings.ucode == ucode_S2DEX)
   {
      ys_memrect(w0, w1);
      return;
   }

   if (rdp.skip_drawing || (!fb_emulation_enabled && (rdp.cimg == rdp.zimg)))
   {
      if ((settings.hacks&hack_PMario) && rdp.ci_status == CI_USELESS)
         pm_palette_mod ();
      return;
   }

   if ((settings.ucode == ucode_PerfectDark) && (rdp.frame_buffers[rdp.ci_count-1].status == CI_ZCOPY))
   {
      pd_zcopy(w0, w1);
      LRDP("Depth buffer copied.\n");
      return;
   }

   if ((rdp.othermode_l >> 16) == 0x3c18 && rdp.cycle1 == 0x03ffffff && rdp.cycle2 == 0x01ff1fff) //depth image based fog
   {
      if (!depth_buffer_fog)
         return;
      if (settings.fog)
         DrawDepthBufferFog();
      depth_buffer_fog = false;
      return;
   }

   // FRDP ("rdp.cycle1 %08lx, rdp.cycle2 %08lx\n", rdp.cycle1, rdp.cycle2);

   if (((rdp.othermode_h & RDP_CYCLE_TYPE) >> 20) == 2)
   {
      ul_x = (short)((rdp.cmd1 & 0x00FFF000) >> 14);
      ul_y = (short)((rdp.cmd1 & 0x00000FFF) >> 2);
      lr_x = (short)((rdp.cmd0 & 0x00FFF000) >> 14);
      lr_y = (short)((rdp.cmd0 & 0x00000FFF) >> 2);
   }
   else
   {
      ul_x = ((short)((rdp.cmd1 & 0x00FFF000) >> 12)) / 4.0f;
      ul_y = ((short)(rdp.cmd1 & 0x00000FFF)) / 4.0f;
      lr_x = ((short)((rdp.cmd0 & 0x00FFF000) >> 12)) / 4.0f;
      lr_y = ((short)(rdp.cmd0 & 0x00000FFF)) / 4.0f;
   }

   if (ul_x >= lr_x)
   {
      FRDP("Wrong Texrect: ul_x: %f, lr_x: %f\n", ul_x, lr_x);
      return;
   }

   if (((rdp.othermode_h & RDP_CYCLE_TYPE) >> 20) > 1)
   {
      lr_x += 1.0f;
      lr_y += 1.0f;
   } else if (lr_y - ul_y < 1.0f)
      lr_y = ceil(lr_y);

   if (settings.increase_texrect_edge)
   {
      if (floor(lr_x) != lr_x)
         lr_x = ceil(lr_x);
      if (floor(lr_y) != lr_y)
         lr_y = ceil(lr_y);
   }

   //*/
   // framebuffer workaround for Zelda: MM LOT
   if ((rdp.othermode_l & 0xFFFF0000) == 0x0f5a0000)
      return;

   /*Gonetz*/
   //hack for Zelda MM. it removes black texrects which cover all geometry in "Link meets Zelda" cut scene
   if ((settings.hacks&hack_Zelda) && rdp.timg.addr >= rdp.cimg && rdp.timg.addr < rdp.ci_end)
   {
      FRDP("Wrong Texrect. texaddr: %08lx, cimg: %08lx, cimg_end: %08lx\n", rdp.cur_cache[0]->addr, rdp.cimg, rdp.cimg+rdp.ci_width*rdp.ci_height*2);
      return;
   }
   //*
   //hack for Banjo2. it removes black texrects under Banjo
   if (!fb_hwfbe_enabled && ((rdp.cycle1 << 16) | (rdp.cycle2 & 0xFFFF)) == 0xFFFFFFFF && (rdp.othermode_l & 0xFFFF0000) == 0x00500000)
   {
      return;
   }
   //*/
   //*
   //remove motion blur in night vision
   if ((settings.ucode == ucode_PerfectDark) && (rdp.maincimg[1].addr != rdp.maincimg[0].addr) && (rdp.timg.addr >= rdp.maincimg[1].addr) && (rdp.timg.addr < (rdp.maincimg[1].addr+rdp.ci_width*rdp.ci_height*rdp.ci_size)))
   {
      if (fb_emulation_enabled)
         if (rdp.frame_buffers[rdp.ci_count-1].status == CI_COPY_SELF)
         {
            //FRDP("Wrong Texrect. texaddr: %08lx, cimg: %08lx, cimg_end: %08lx\n", rdp.timg.addr, rdp.maincimg[1], rdp.maincimg[1]+rdp.ci_width*rdp.ci_height*rdp.ci_size);
            LRDP("Wrong Texrect.\n");
            return;
         }
   }
   //*/


   tile = (uint16_t)((w1 & 0x07000000) >> 24);

   rdp.texrecting = 1;

   prev_tile = rdp.cur_tile;
   rdp.cur_tile = tile;

   Z = set_sprite_combine_mode ();

   rdp.texrecting = 0;

   if (!rdp.cur_cache[0])
   {
      rdp.cur_tile = prev_tile;
      return;
   }
   // ****
   // ** Texrect offset by Gugaman **
   //
   //integer representation of texture coordinate.
   //needed to detect and avoid overflow after shifting
   off_x_i = (rdp.cmd2 >> 16) & 0xFFFF;
   off_y_i = rdp.cmd2 & 0xFFFF;
   dsdx = (float)((int16_t)((rdp.cmd3 & 0xFFFF0000) >> 16)) / 1024.0f;
   dtdy = (float)((int16_t)(rdp.cmd3 & 0x0000FFFF)) / 1024.0f;
   if (off_x_i & 0x8000) //check for sign bit
      off_x_i |= ~0xffff; //make it negative
   //the same as for off_x_i
   if (off_y_i & 0x8000)
      off_y_i |= ~0xffff;

   if (((rdp.othermode_h & RDP_CYCLE_TYPE) >> 20) == 2)
      dsdx /= 4.0f;

   s_ul_x = ul_x * rdp.scale_x + rdp.offset_x;
   s_lr_x = lr_x * rdp.scale_x + rdp.offset_x;
   s_ul_y = ul_y * rdp.scale_y + rdp.offset_y;
   s_lr_y = lr_y * rdp.scale_y + rdp.offset_y;

   if ( ((rdp.cmd0>>24)&0xFF) == 0xE5 ) //texrectflip
   {
      off_size_x = (lr_y - ul_y - 1) * dsdx;
      off_size_y = (lr_x - ul_x - 1) * dtdy;
   }
   else
   {
      off_size_x = (lr_x - ul_x - 1) * dsdx;
      off_size_y = (lr_y - ul_y - 1) * dtdy;
   }

   //calculate texture coordinates
   for (i = 0; i < 2; i++)
   {
      if (rdp.cur_cache[i] && (rdp.tex & (i+1)))
      {
         int x_i, y_i;
         TILE *tile;
         float sx, sy;

         sx = 1;
         sy = 1;
         x_i = off_x_i;
         y_i = off_y_i;
         tile = (TILE*)&rdp.tiles[rdp.cur_tile + i];

         //shifting
         if (tile->shift_s)
         {
            if (tile->shift_s > 10)
            {
               uint8_t iShift = (16 - tile->shift_s);
               x_i <<= iShift;
               sx = (float)(1 << iShift);
            }
            else
            {
               uint8_t iShift = tile->shift_s;
               x_i >>= iShift;
               sx = 1.0f/(float)(1 << iShift);
            }
         }
         if (tile->shift_t)
         {
            if (tile->shift_t > 10)
            {
               uint8_t iShift = (16 - tile->shift_t);
               y_i <<= iShift;
               sy = (float)(1 << iShift);
            }
            else
            {
               uint8_t iShift = tile->shift_t;
               y_i >>= iShift;
               sy = 1.0f/(float)(1 << iShift);
            }
         }

         {
            //kill 10.5 format overflow by SIGN16 macro
            texUV[i].ul_u = SIGN16(x_i) / 32.0f;
            texUV[i].ul_v = SIGN16(y_i) / 32.0f;

            texUV[i].ul_u -= tile->f_ul_s;
            texUV[i].ul_v -= tile->f_ul_t;

            texUV[i].lr_u = texUV[i].ul_u + off_size_x * sx;
            texUV[i].lr_v = texUV[i].ul_v + off_size_y * sy;

            texUV[i].ul_u = rdp.cur_cache[i]->c_off + rdp.cur_cache[i]->c_scl_x * texUV[i].ul_u;
            texUV[i].lr_u = rdp.cur_cache[i]->c_off + rdp.cur_cache[i]->c_scl_x * texUV[i].lr_u;
            texUV[i].ul_v = rdp.cur_cache[i]->c_off + rdp.cur_cache[i]->c_scl_y * texUV[i].ul_v;
            texUV[i].lr_v = rdp.cur_cache[i]->c_off + rdp.cur_cache[i]->c_scl_y * texUV[i].lr_v;
         }
      }
      else
      {
         texUV[i].ul_u = texUV[i].ul_v = texUV[i].lr_u = texUV[i].lr_v = 0;
      }
   }
   rdp.cur_tile = prev_tile;

   // ****

   FRDP (" scissor: (%d, %d) -> (%d, %d)\n", rdp.scissor.ul_x, rdp.scissor.ul_y, rdp.scissor.lr_x, rdp.scissor.lr_y);

   CCLIP2 (s_ul_x, s_lr_x, texUV[0].ul_u, texUV[0].lr_u, texUV[1].ul_u, texUV[1].lr_u, (float)rdp.scissor.ul_x, (float)rdp.scissor.lr_x);
   CCLIP2 (s_ul_y, s_lr_y, texUV[0].ul_v, texUV[0].lr_v, texUV[1].ul_v, texUV[1].lr_v, (float)rdp.scissor.ul_y, (float)rdp.scissor.lr_y);

   FRDP (" draw at: (%f, %f) -> (%f, %f)\n", s_ul_x, s_ul_y, s_lr_x, s_lr_y);

   memset(vstd, 0, sizeof(VERTEX) * 4);

   vstd[0].x = s_ul_x;
   vstd[0].y = s_ul_y;
   vstd[0].z = Z;
   vstd[0].q = 1.0f;
   vstd[0].u0 = texUV[0].ul_u;
   vstd[0].v0 = texUV[0].ul_v;
   vstd[0].u1 = texUV[1].ul_u;
   vstd[0].v1 = texUV[1].ul_v;
   vstd[0].coord[0] = 0;
   vstd[0].coord[1] = 0;
   vstd[0].coord[2] = 0;
   vstd[0].coord[3] = 0;
   vstd[0].f = 255.0f;

   vstd[1].x = s_lr_x;
   vstd[1].y = s_ul_y;
   vstd[1].z = Z;
   vstd[1].q = 1.0f;
   vstd[1].u0 = texUV[0].lr_u;
   vstd[1].v0 = texUV[0].ul_v;
   vstd[1].u1 = texUV[1].lr_u;
   vstd[1].v1 = texUV[1].ul_v;
   vstd[1].coord[0] = 0;
   vstd[1].coord[1] = 0;
   vstd[1].coord[2] = 0;
   vstd[1].coord[3] = 0;
   vstd[1].f = 255.0f;

   vstd[2].x = s_ul_x;
   vstd[2].y = s_lr_y;
   vstd[2].z = Z;
   vstd[2].q = 1.0f;
   vstd[2].u0 = texUV[0].ul_u;
   vstd[2].v0 = texUV[0].lr_v;
   vstd[2].u1 = texUV[1].ul_u;
   vstd[2].v1 = texUV[1].lr_v;
   vstd[2].coord[0] = 0;
   vstd[2].coord[1] = 0;
   vstd[2].coord[2] = 0;
   vstd[2].coord[3] = 0;
   vstd[2].f = 255.0f;

   vstd[3].x = s_lr_x;
   vstd[3].y = s_lr_y;
   vstd[3].z = Z;
   vstd[3].q = 1.0f;
   vstd[3].u0 = texUV[0].lr_u;
   vstd[3].v0 = texUV[0].lr_v;
   vstd[3].u1 = texUV[1].lr_u;
   vstd[3].v1 = texUV[1].lr_v;
   vstd[3].coord[0] = 0;
   vstd[3].coord[1] = 0;
   vstd[3].coord[2] = 0;
   vstd[3].coord[3] = 0;
   vstd[3].f = 255.0f;

   if ( ((rdp.cmd0>>24)&0xFF) == 0xE5 ) //texrectflip
   {
      vstd[1].u0 = texUV[0].ul_u;
      vstd[1].v0 = texUV[0].lr_v;
      vstd[1].u1 = texUV[1].ul_u;
      vstd[1].v1 = texUV[1].lr_v;

      vstd[2].u0 = texUV[0].lr_u;
      vstd[2].v0 = texUV[0].ul_v;
      vstd[2].u1 = texUV[1].lr_u;
      vstd[2].v1 = texUV[1].ul_v;
   }


   vptr = (VERTEX*)vstd;

   apply_shading(vptr);

#if 0
   if (fullscreen)
#endif
   {
      if (rdp.fog_mode >= FOG_MODE_BLEND)
      {
         float fog = 1.0f/ rdp.fog_color_sep[3];
         if (rdp.fog_mode != FOG_MODE_BLEND)
            fog = 1.0f / ((~rdp.fog_color)&0xFF);

         for (i = 0; i < 4; i++)
            vptr[i].f = fog;
         grFogMode (GR_FOG_WITH_TABLE_ON_FOGCOORD_EXT, rdp.fog_color);
      }

      ConvertCoordsConvert (vptr, 4);
      grDrawVertexArrayContiguous (GR_TRIANGLE_STRIP, 4, vptr);
   }
}

static void rdp_loadsync(uint32_t w0, uint32_t w1)
{
}

static void rdp_pipesync(uint32_t w0, uint32_t w1)
{
}

static void rdp_tilesync(uint32_t w0, uint32_t w1)
{
}

static void rdp_fullsync(uint32_t w0, uint32_t w1)
{
   *gfx_info.MI_INTR_REG |= 0x20;
   gfx_info.CheckInterrupts();
}

static void rdp_setkeygb(uint32_t w0, uint32_t w1)
{
   rdp.SCALE = (rdp.SCALE & 0xFF0000FF) | (((w1 >> 16) & 0xFF) << 16)   | (((w1 & 0xFF)) << 8);
   rdp.CENTER = (rdp.CENTER & 0xFF0000FF) | (((w1 >> 24) & 0xFF) << 16) | (((w1 >> 8) & 0xFF) << 8);
   rdp.key_width[1]  = (w0 & 0x00FFF000) >> 12;
   rdp.key_width[2]  = (w0 & 0x00000FFF) >>  0;
   rdp.key_center[1] = (w1 & 0xFF000000) >> 24;
   rdp.key_center[2] = (w1 & 0x0000FF00) >>  8;
   rdp.key_scale[1]  = (w1 & 0x00FF0000) >> 16;
   rdp.key_scale[2]  = (w1 & 0x000000FF) >>  0;
}

static void rdp_setkeyr(uint32_t w0, uint32_t w1)
{
   rdp.SCALE  = (rdp.SCALE & 0x00FFFFFF)  | ((w1 & 0xFF) << 24);
   rdp.CENTER = (rdp.CENTER & 0x00FFFFFF) | (((w1 >> 8) & 0xFF) << 24);
   rdp.key_width[0]  = (w1 & 0x0FFF0000) >> 16;
   rdp.key_center[0] = (w1 & 0x0000FF00) >>  8;
   rdp.key_scale[0]  = (w1 & 0x000000FF) >>  0;
}

static void rdp_setconvert(uint32_t w0, uint32_t w1)
{
   rdp.K4 = (uint8_t)(w1 >> 9) & 0x1FF;
   rdp.K5 = (uint8_t)(w1 & 0x1FF);
   //FRDP("setconvert. K4=%02lx K5=%02lx\n", rdp.K4, rdp.K5);
}

static void rdp_setscissor(uint32_t w0, uint32_t w1)
{
   // clipper resolution is 320x240, scale based on computer resolution
   rdp.scissor_o.ul_x = (uint32_t)(((w0 & 0x00FFF000) >> 14));
   rdp.scissor_o.ul_y = (uint32_t)(((w0 & 0x00000FFF) >> 2));
   rdp.scissor_o.lr_x = (uint32_t)(((w1 & 0x00FFF000) >> 14));
   rdp.scissor_o.lr_y = (uint32_t)(((w1 & 0x00000FFF) >> 2));

   rdp.ci_upper_bound = rdp.scissor_o.ul_y;
   rdp.ci_lower_bound = rdp.scissor_o.lr_y;
   rdp.scissor_set = true;

   //FRDP("setscissor: (%d,%d) -> (%d,%d)\n", rdp.scissor_o.ul_x, rdp.scissor_o.ul_y, rdp.scissor_o.lr_x, rdp.scissor_o.lr_y);

   rdp.update |= UPDATE_SCISSOR;

   if (rdp.view_scale[0] == 0) //viewport is not set?
   {
      rdp.view_scale[0] = (rdp.scissor_o.lr_x>>1) * rdp.scale_x;
      rdp.view_scale[1] = (rdp.scissor_o.lr_y>>1) * -rdp.scale_y;
      rdp.view_trans[0] = rdp.view_scale[0];
      rdp.view_trans[1] = -rdp.view_scale[1];
      rdp.update |= UPDATE_VIEWPORT;
   }
}

static void rdp_setprimdepth(uint32_t w0, uint32_t w1)
{
   rdp.prim_depth = (uint16_t)((w1 >> 16) & 0x7FFF);
   rdp.prim_dz = (uint16_t)(w1 & 0x7FFF);
}

#define F3DEX2_SETOTHERMODE(cmd,sft,len,data) { \
   rdp.cmd0 = (cmd<<24) | ((32-(sft)-(len))<<8) | (((len)-1)); \
   rdp.cmd1 = data; \
   gfx_instruction[settings.ucode][cmd](rdp.cmd0, rdp.cmd1); \
}
#define SETOTHERMODE(cmd,sft,len,data) { \
   rdp.cmd0 = (cmd<<24) | ((sft)<<8) | (len); \
   rdp.cmd1 = data; \
   gfx_instruction[settings.ucode][cmd](rdp.cmd0, rdp.cmd1); \
}

static void rdp_setothermode(uint32_t w0, uint32_t w1)
{

   LRDP("rdp_setothermode\n");

   if ((settings.ucode == ucode_F3DEX2) || (settings.ucode == ucode_CBFD))
   {
      int cmd0 = rdp.cmd0;
      F3DEX2_SETOTHERMODE(0xE2, 0, 32, rdp.cmd1); // SETOTHERMODE_L
      F3DEX2_SETOTHERMODE(0xE3, 0, 32, cmd0 & 0x00FFFFFF); // SETOTHERMODE_H
   }
   else
   {
      int cmd0 = rdp.cmd0;
      SETOTHERMODE(0xB9, 0, 32, rdp.cmd1); // SETOTHERMODE_L
      SETOTHERMODE(0xBA, 0, 32, cmd0 & 0x00FFFFFF); // SETOTHERMODE_H
   }
}

void load_palette (uint32_t addr, uint16_t start, uint16_t count)
{
   uint16_t *dpal, end, i, p;
   dpal = (uint16_t*)(rdp.pal_8 + start);
   end = start+count;

   for (i=start; i<end; i++)
   {
      *(dpal++) = *(uint16_t *)(gfx_info.RDRAM + (addr^2));
      addr += 2;
      //FRDP ("%d: %08lx\n", i, *(uint16_t *)(gfx_info.RDRAM + (addr^2)));
   }
   start >>= 4;
   end = start + (count >> 4);
   if (end == start) // it can be if count < 16
      end = start + 1;
   for (p = start; p < end; p++)
      rdp.pal_8_crc[p] = CRC32( 0xFFFFFFFF, &rdp.pal_8[(p << 4)], 32 );
   rdp.pal_256_crc = CRC32( 0xFFFFFFFF, rdp.pal_8_crc, 64 );
}

static void rdp_loadtlut(uint32_t w0, uint32_t w1)
{
   int32_t i, j;
   uint32_t tile;
   uint16_t start, count;

   tile = (w1 >> 24) & 0x07;
   start = rdp.tiles[tile].t_mem - 256; // starting location in the palettes
   count = ((uint16_t)(w1 >> 14) & 0x3FF) + 1;    // number to copy

   if (rdp.timg.addr + (count<<1) > BMASK)
      count = (uint16_t)((BMASK - rdp.timg.addr) >> 1);

   if (start+count > 256)
      count = 256-start;

   load_palette (rdp.timg.addr, start, count);

   rdp.timg.addr += count << 1;

   //FRDP("loadtlut: tile: %d, start: %d, count: %d, from: %08lx\n", tile, start, count, rdp.timg.addr);
}

static void rdp_settilesize(uint32_t w0, uint32_t w1)
{
   int ul_s, ul_t, lr_s, lr_t, tilenum;

   ul_s    = (((uint16_t)(w0 >> 14)) & 0x03ff);
   ul_t    = (((uint16_t)(w0 >> 2 )) & 0x03ff);
   tilenum = (w1 >> 24) & 0x07;
   lr_s    = (((uint16_t)(w1 >> 14)) & 0x03ff);
   lr_t    = (((uint16_t)(w1 >> 2 )) & 0x03ff);

   rdp.last_tile_size = tilenum;

   rdp.tiles[tilenum].f_ul_s = (float)((w0 >> 12) & 0xFFF) / 4.0f;
   rdp.tiles[tilenum].f_ul_t = (float)(w0 & 0xFFF) / 4.0f;

   rdp.tiles[tilenum].ul_s = ul_s;
   rdp.tiles[tilenum].ul_t = ul_t;
   rdp.tiles[tilenum].lr_s = lr_s;
   rdp.tiles[tilenum].lr_t = lr_t;

   // handle wrapping
   if (rdp.tiles[tilenum].lr_s < rdp.tiles[tilenum].ul_s)
      rdp.tiles[tilenum].lr_s += 0x400;
   if (rdp.tiles[tilenum].lr_t < rdp.tiles[tilenum].ul_t)
      rdp.tiles[tilenum].lr_t += 0x400;

   rdp.update |= UPDATE_TEXTURE;
}


static INLINE void loadTile(uint32_t *src, uint32_t *dst, int width, int height, int line, int off, uint32_t *end)
{
   uint32_t *v7, *v9, *v13, v16, *v17, v18, v20, v22, *v24, *v27, *v31, nbits;
   int v8, v10, v11, v12, v14, v15, v19, v21, v23, v25, v26, v28, v29, v30;
   nbits = sizeof(uint32_t) * 8;

   v7 = dst;
   v8 = width;
   v9 = src;
   v10 = off;
   v11 = 0;
   v12 = height;
   do
   {
      if ( end < v7 )
         break;
      v31 = v7;
      v30 = v8;
      v29 = v12;
      v28 = v11;
      v27 = v9;
      v26 = v10;
      if ( v8 )
      {
         v25 = v8;
         v24 = v9;
         v23 = v10;
         v13 = (uint32_t *)((char *)v9 + (v10 & 0xFFFFFFFC));
         v14 = v10 & 3;
         if ( !(v10 & 3) )
            goto LABEL_20;
         v15 = 4 - v14;
         v16 = *v13;
         v17 = v13 + 1;
         do
         {
            v16 = __ROL__(v16, 8, nbits);
            --v14;
         }
         while ( v14 );
         do
         {
            v16 = __ROL__(v16, 8, nbits);
            *(uint8_t *)v7 = v16;
            v7 = (uint32_t *)((char *)v7 + 1);
            --v15;
         }
         while ( v15 );
         v18 = *v17;
         v13 = v17 + 1;
         *v7 = bswap32(v18);
         ++v7;
         --v8;
         if ( v8 )
         {
LABEL_20:
            do
            {
               *v7 = bswap32(*v13);
               v7[1] = bswap32(v13[1]);
               v13 += 2;
               v7 += 2;
               --v8;
            }
            while ( v8 );
         }
         v19 = v23 & 3;
         if ( v23 & 3 )
         {
            v20 = *(uint32_t *)((char *)v24 + ((8 * v25 + v23) & 0xFFFFFFFC));
            do
            {
               v20 = __ROL__(v20, 8, nbits);
               *(uint8_t *)v7 = v20;
               v7 = (uint32_t *)((char *)v7 + 1);
               --v19;
            }
            while ( v19 );
         }
      }
      v9 = v27;
      v21 = v29;
      v8 = v30;
      v11 = v28 ^ 1;
      if ( v28 == 1 )
      {
         v7 = v31;
         if ( v30 )
         {
            do
            {
               v22 = *v7;
               *v7 = v7[1];
               v7[1] = v22;
               v7 += 2;
               --v8;
            }
            while ( v8 );
         }
         v21 = v29;
         v8 = v30;
      }
      v10 = line + v26;
      v12 = v21 - 1;
   }
   while ( v12 );
}

static void rdp_loadblock(uint32_t w0, uint32_t w1)
{
   // lr_s specifies number of 64-bit words to copy
   // 10.2 format
   gDPLoadBlock(
         ((w1 >> 24) & 0x07), 
         (w0 >> 14) & 0x3FF, /* ul_s */
         (w0 >>  2) & 0x3FF, /* ul_t */
         (w1 >> 14) & 0x3FF, /* lr_s */
         (w1 & 0x0FFF) /* dxt */
         );
}

void LoadTile32b (uint32_t tile, uint32_t ul_s, uint32_t ul_t, uint32_t width, uint32_t height);

static void rdp_loadtile(uint32_t w0, uint32_t w1)
{
   uint32_t tile, offs, height, width;
   uint16_t ul_s, ul_t, lr_s, lr_t;
   int line_n;
   
   if (rdp.skip_drawing)
      return;

   rdp.timg.set_by = 1; // load tile

   tile = (uint32_t)((w1 >> 24) & 0x07);

   rdp.addr[rdp.tiles[tile].t_mem] = rdp.timg.addr;

   ul_s = (uint16_t)((w0 >> 14) & 0x03FF);
   ul_t = (uint16_t)((w0 >> 2 ) & 0x03FF);
   lr_s = (uint16_t)((w1 >> 14) & 0x03FF);
   lr_t = (uint16_t)((w1 >> 2 ) & 0x03FF);

   if (lr_s < ul_s || lr_t < ul_t)
      return;


   if ((settings.hacks&hack_Tonic) && tile == 7)
   {
      rdp.tiles[0].ul_s = ul_s;
      rdp.tiles[0].ul_t = ul_t;
      rdp.tiles[0].lr_s = lr_s;
      rdp.tiles[0].lr_t = lr_t;
   }

   height = lr_t - ul_t + 1; // get height
   width = lr_s - ul_s + 1;

#ifdef TEXTURE_FILTER
   LOAD_TILE_INFO &info = rdp.load_info[rdp.tiles[tile].t_mem];
   info.tile_ul_s = ul_s;
   info.tile_ul_t = ul_t;
   info.tile_width = (rdp.tiles[tile].mask_s ? min((uint16_t)width, 1<<rdp.tiles[tile].mask_s) : (uint16_t)width);
   info.tile_height = (rdp.tiles[tile].mask_t ? min((uint16_t)height, 1<<rdp.tiles[tile].mask_t) : (uint16_t)height);
   if (settings.hacks&hack_MK64) {
      if (info.tile_width%2)
         info.tile_width--;
      if (info.tile_height%2)
         info.tile_height--;
   }
   info.tex_width = rdp.timg.width;
   info.tex_size = rdp.timg.size;
#endif


   line_n = rdp.timg.width << rdp.tiles[tile].size >> 1;
   offs = ul_t * line_n;
   offs += ul_s << rdp.tiles[tile].size >> 1;
   offs += rdp.timg.addr;
   if (offs >= BMASK)
      return;

   if (rdp.timg.size == 3)
   {
      LoadTile32b(tile, ul_s, ul_t, width, height);
   }
   else
   {
      uint8_t *dst, *end;
      uint32_t wid_64;

      // check if points to bad location
      if (offs + line_n*height > BMASK)
         height = (BMASK - offs) / line_n;
      if (height == 0)
         return;

      wid_64 = rdp.tiles[tile].line;
      dst = ((uint8_t*)rdp.tmem) + (rdp.tiles[tile].t_mem<<3);
      end = ((uint8_t*)rdp.tmem) + 4096 - (wid_64<<3);
      loadTile((uint32_t *)gfx_info.RDRAM, (uint32_t *)dst, wid_64, height, line_n, offs, (uint32_t *)end);
   }
   //FRDP("loadtile: tile: %d, ul_s: %d, ul_t: %d, lr_s: %d, lr_t: %d\n", tile, ul_s, ul_t, lr_s, lr_t);

}

static void rdp_settile(uint32_t w0, uint32_t w1)
{
   TILE *tile;

   rdp.last_tile = (uint32_t)((w1 >> 24) & 0x07);
   tile = (TILE*)&rdp.tiles[rdp.last_tile];

   tile->format    = (uint8_t)((w0 >> 21) & 0x07);
   tile->size      = (uint8_t)((w0 >> 19) & 0x03);
   tile->line      = (uint16_t)((w0 >> 9) & 0x01FF);
   tile->t_mem     = (uint16_t)(w0 & 0x1FF);
   tile->palette   = (uint8_t)((w1 >> 20) & 0x0F);
   tile->clamp_t   = (uint8_t)((w1 >> 19) & 0x01);
   tile->mirror_t  = (uint8_t)((w1 >> 18) & 0x01);
   tile->mask_t    = (uint8_t)((w1 >> 14) & 0x0F);
   tile->shift_t   = (uint8_t)((w1 >> 10) & 0x0F);
   tile->clamp_s   = (uint8_t)((w1 >> 9) & 0x01);
   tile->mirror_s  = (uint8_t)((w1 >> 8) & 0x01);
   tile->mask_s    = (uint8_t)((w1 >> 4) & 0x0F);
   tile->shift_s   = (uint8_t)(w1 & 0x0F);

   rdp.update |= UPDATE_TEXTURE;
}

static void rdp_fillrect(uint32_t w0, uint32_t w1)
{
   int32_t s_ul_x, s_lr_x, s_ul_y, s_lr_y;
   int pd_multiplayer;

   uint32_t ul_x = ((w1 & 0x00FFF000) >> 14);
   uint32_t ul_y = (w1 & 0x00000FFF) >> 2;
   uint32_t lr_x = ((w0 & 0x00FFF000) >> 14) + 1;
   uint32_t lr_y = ((w0 & 0x00000FFF) >> 2) + 1;

   if ((ul_x > lr_x) || (ul_y > lr_y))
   {
      //LRDP("Fillrect. Wrong coordinates. Skipped\n");
      return;
   }

   pd_multiplayer = (settings.ucode == ucode_PerfectDark) && (((rdp.othermode_h & RDP_CYCLE_TYPE) >> 20) == 3) && (rdp.fill_color == 0xFFFCFFFC);
   if ((rdp.cimg == rdp.zimg) || (fb_emulation_enabled && rdp.frame_buffers[rdp.ci_count-1].status == CI_ZIMG) || pd_multiplayer)
   {
      //LRDP("Fillrect - cleared the depth buffer\n");
      //if (fullscreen)
      {
         if (!(settings.hacks&hack_Hyperbike) || rdp.ci_width > 64) //do not clear main depth buffer for aux depth buffers
         {
            update_scissor(false);
            grDepthMask (FXTRUE);
            grColorMask (FXFALSE, FXFALSE);
            grBufferClear (0, 0, rdp.fill_color ? rdp.fill_color&0xFFFF : 0xFFFF);
            grColorMask (FXTRUE, FXTRUE);
            rdp.update |= UPDATE_ZBUF_ENABLED;
         }
         //if (settings.frame_buffer&fb_depth_clear)
         {
            uint32_t zi_width_in_dwords, *dst, x, y;
            ul_x = min(max(ul_x, rdp.scissor_o.ul_x), rdp.scissor_o.lr_x);
            lr_x = min(max(lr_x, rdp.scissor_o.ul_x), rdp.scissor_o.lr_x);
            ul_y = min(max(ul_y, rdp.scissor_o.ul_y), rdp.scissor_o.lr_y);
            lr_y = min(max(lr_y, rdp.scissor_o.ul_y), rdp.scissor_o.lr_y);
            zi_width_in_dwords = rdp.ci_width >> 1;
            ul_x >>= 1;
            lr_x >>= 1;
            dst = (uint32_t*)(gfx_info.RDRAM+rdp.cimg);
            dst += ul_y * zi_width_in_dwords;
            for (y = ul_y; y < lr_y; y++)
            {
               for (x = ul_x; x < lr_x; x++)
                  dst[x] = rdp.fill_color;
               dst += zi_width_in_dwords;
            }
         }
      }
      return;
   }

   if (rdp.skip_drawing)
   {
      //LRDP("Fillrect skipped\n");
      return;
   }

   // Update scissor
   //if (fullscreen)
   update_scissor(false);

   if (settings.decrease_fillrect_edge && ((rdp.othermode_h & RDP_CYCLE_TYPE) >> 20) == 0)
   {
      lr_x--; lr_y--;
   }

   s_ul_x = (uint32_t)(ul_x * rdp.scale_x + rdp.offset_x);
   s_lr_x = (uint32_t)(lr_x * rdp.scale_x + rdp.offset_x);
   s_ul_y = (uint32_t)(ul_y * rdp.scale_y + rdp.offset_y);
   s_lr_y = (uint32_t)(lr_y * rdp.scale_y + rdp.offset_y);

   if (s_lr_x < 0)
      s_lr_x = 0;
   if (s_lr_y < 0)
      s_lr_y = 0;
   if ((uint32_t)s_ul_x > settings.res_x)
      s_ul_x = settings.res_x;
   if ((uint32_t)s_ul_y > settings.res_y)
      s_ul_y = settings.res_y;

   FRDP (" - %d, %d, %d, %d\n", s_ul_x, s_ul_y, s_lr_x, s_lr_y);

   {
      VERTEX v[4], vout[4], vout2[4];
      float Z;

      grFogMode (GR_FOG_DISABLE, rdp.fog_color);

      Z = (((rdp.othermode_h & RDP_CYCLE_TYPE) >> 20) == 3) ? 0.0f : set_sprite_combine_mode();

      memset(v, 0, sizeof(VERTEX) * 4);

      // Draw the vertices
      v[0].x = (float)s_ul_x;
      v[0].y = (float)s_ul_y;
      v[0].z = Z;
      v[0].q = 1.0f;
      v[1].x = (float)s_lr_x;
      v[1].y = (float)s_ul_y;
      v[1].z = Z;
      v[1].q = 1.0f;
      v[2].x = (float)s_ul_x;
      v[2].y = (float)s_lr_y;
      v[2].z = Z;
      v[2].q = 1.0f;
      v[3].x = (float)s_lr_x;
      v[3].y = (float)s_lr_y;
      v[3].z = Z;
      v[3].q = 1.0f;

      if (((rdp.othermode_h & RDP_CYCLE_TYPE) >> 20) == 3)
      {
         uint32_t color = rdp.fill_color;

         if ((settings.hacks&hack_PMario) && rdp.frame_buffers[rdp.ci_count-1].status == CI_AUX)
         {
            //background of auxiliary frame buffers must have zero alpha.
            //make it black, set 0 alpha to plack pixels on frame buffer read
            color = 0;
         }
         else if (rdp.ci_size < 3)
         {
            color = ((color&1)?0xFF:0) |
               ((uint32_t)((float)((color&0xF800) >> 11) / 31.0f * 255.0f) << 24) |
               ((uint32_t)((float)((color&0x07C0) >> 6) / 31.0f * 255.0f) << 16) |
               ((uint32_t)((float)((color&0x003E) >> 1) / 31.0f * 255.0f) << 8);
         }

         grConstantColorValue (color);

         grColorCombine (GR_COMBINE_FUNCTION_LOCAL,
               GR_COMBINE_FACTOR_NONE,
               GR_COMBINE_LOCAL_CONSTANT,
               GR_COMBINE_OTHER_NONE,
               FXFALSE);

         grAlphaCombine (GR_COMBINE_FUNCTION_LOCAL,
               GR_COMBINE_FACTOR_NONE,
               GR_COMBINE_LOCAL_CONSTANT,
               GR_COMBINE_OTHER_NONE,
               FXFALSE);

         grAlphaBlendFunction (GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);

         grAlphaTestFunction (GR_CMP_ALWAYS, 0x00, 0);
         grStippleMode(GR_STIPPLE_DISABLE);

         grCullMode(GR_CULL_DISABLE);
         grFogMode (GR_FOG_DISABLE, rdp.fog_color);
         grDepthBufferFunction (GR_CMP_ALWAYS);
         grDepthMask (FXFALSE);

         rdp.update |= UPDATE_COMBINE | UPDATE_CULL_MODE | UPDATE_FOG_ENABLED | UPDATE_ZBUF_ENABLED;
      }
      else
      {
         uint32_t cmb_mode_c, cmb_mode_a;
         cmb_mode_c = (rdp.cycle1 << 16) | (rdp.cycle2 & 0xFFFF);
         cmb_mode_a = (rdp.cycle1 & 0x0FFF0000) | ((rdp.cycle2 >> 16) & 0x00000FFF);

         if (cmb_mode_c == 0x9fff9fff || cmb_mode_a == 0x09ff09ff) //shade
            apply_shading(v);

         if ((rdp.othermode_l & RDP_FORCE_BLEND) && ((rdp.othermode_l >> 16) == 0x0550)) //special blender mode for Bomberman64
         {
            grAlphaCombine (GR_COMBINE_FUNCTION_LOCAL,
                  GR_COMBINE_FACTOR_NONE,
                  GR_COMBINE_LOCAL_CONSTANT,
                  GR_COMBINE_OTHER_NONE,
                  FXFALSE);
            grConstantColorValue((cmb.ccolor & 0xFFFFFF00) | rdp.fog_color_sep[3]);
            rdp.update |= UPDATE_COMBINE;
         }
      }

      vout[0]  = v[0];
      vout[1]  = v[2];
      vout[2]  = v[1];
      vout2[0] = v[2];
      vout2[1] = v[3];
      vout2[2] = v[1];

      grDrawVertexArrayContiguous (GR_TRIANGLE_STRIP, 3, &vout[0]);
      grDrawVertexArrayContiguous (GR_TRIANGLE_STRIP, 3, &vout2[0]);
   }
}

static void rdp_setfillcolor(uint32_t w0, uint32_t w1)
{
   rdp.fill_color = w1;
   rdp.update |= UPDATE_ALPHA_COMPARE | UPDATE_COMBINE;
}

static void rdp_setfogcolor(uint32_t w0, uint32_t w1)
{
   rdp.fog_color = w1;
   rdp.fog_color_sep[0] = (w1 & 0xFF000000) >> 24;
   rdp.fog_color_sep[1] = (w1 & 0x00FF0000) >> 16;
   rdp.fog_color_sep[2] = (w1 & 0x0000FF00) >>  8;
   rdp.fog_color_sep[3] = (w1 & 0x000000FF) >>  0;
   rdp.update |= UPDATE_COMBINE | UPDATE_FOG_ENABLED;
}

static void rdp_setblendcolor(uint32_t w0, uint32_t w1)
{
   rdp.blend_color = w1;
   rdp.blend_color_sep[0] = (w1 & 0xFF000000) >> 24;
   rdp.blend_color_sep[1] = (w1 & 0x00FF0000) >> 16;
   rdp.blend_color_sep[2] = (w1 & 0x0000FF00) >>  8;
   rdp.blend_color_sep[3] = (w1 & 0x000000FF) >>  0;
   rdp.update |= UPDATE_COMBINE;
}

static void rdp_setprimcolor(uint32_t w0, uint32_t w1)
{
   rdp.prim_color = w1;
   rdp.prim_color_sep[0] = (w1 & 0xFF000000) >> 24;
   rdp.prim_color_sep[1] = (w1 & 0x00FF0000) >> 16;
   rdp.prim_color_sep[2] = (w1 & 0x0000FF00) >> 8;
   rdp.prim_color_sep[3] = (w1 & 0x000000FF);
   rdp.prim_lodmin = (w0 >> 8) & 0xFF;
   rdp.prim_lodfrac = max(w0 & 0xFF, rdp.prim_lodmin);
   rdp.update |= UPDATE_COMBINE;

   //FRDP("setprimcolor: %08lx, lodmin: %d, lodfrac: %d\n", rdp.cmd1, rdp.prim_lodmin, rdp.prim_lodfrac);
}

static void rdp_setenvcolor(uint32_t w0, uint32_t w1)
{
   rdp.env_color = w1;
   rdp.env_color_sep[0] = (w1 & 0xFF000000) >> 24;
   rdp.env_color_sep[1] = (w1 & 0x00FF0000) >> 16;
   rdp.env_color_sep[2] = (w1 & 0x0000FF00) >> 8;
   rdp.env_color_sep[3] = (w1 & 0x000000FF);
   rdp.update |= UPDATE_COMBINE;

   //FRDP("setenvcolor: %08lx\n", rdp.cmd1);
}

static void rdp_setcombine(uint32_t w0, uint32_t w1)
{
   rdp.c_a0 = (uint8_t)((w0 >> 20) & 0xF);   // sub_a_rgb0
   rdp.c_c0 = (uint8_t)((w0 >> 15) & 0x1F);  // mul_rgb0
   rdp.c_Aa0 = (uint8_t)((w0 >> 12) & 0x7);  // sub_a_a0
   rdp.c_Ac0 = (uint8_t)((w0 >> 9) & 0x7);   // mul_a0
   rdp.c_a1 = (uint8_t)((w0 >> 5) & 0xF);    // sub_a_rgb1
   rdp.c_c1 = (uint8_t)((w0 >> 0) & 0x1F);   // mul_rgb1

   rdp.c_b0 = (uint8_t)((w1 >> 28) & 0xF);   // sub_b_rgb0
   rdp.c_b1 = (uint8_t)((w1 >> 24) & 0xF);   // sub_b_rgb1
   rdp.c_Aa1 = (uint8_t)((w1 >> 21) & 0x7);  // sub_a_a1
   rdp.c_Ac1 = (uint8_t)((w1 >> 18) & 0x7);  // mul_a1
   rdp.c_d0 = (uint8_t)((w1 >> 15) & 0x7);   // add_rgb0
   rdp.c_Ab0 = (uint8_t)((w1 >> 12) & 0x7);  // sub_b_a0
   rdp.c_Ad0 = (uint8_t)((w1 >> 9) & 0x7);   // add_a0
   rdp.c_d1 = (uint8_t)((w1 >> 6) & 0x7);    // add_rgb1
   rdp.c_Ab1 = (uint8_t)((w1 >> 3) & 0x7);   // sub_b_a1
   rdp.c_Ad1 = (uint8_t)((w1 >> 0) & 0x7);   // add_a1

   rdp.cycle1 = (rdp.c_a0<<0) | (rdp.c_b0<<4) | (rdp.c_c0<<8) | (rdp.c_d0<<13)|
      (rdp.c_Aa0<<16)| (rdp.c_Ab0<<19)| (rdp.c_Ac0<<22)| (rdp.c_Ad0<<25);
   rdp.cycle2 = (rdp.c_a1<<0) | (rdp.c_b1<<4) | (rdp.c_c1<<8) | (rdp.c_d1<<13)|
      (rdp.c_Aa1<<16)| (rdp.c_Ab1<<19)| (rdp.c_Ac1<<22)| (rdp.c_Ad1<<25);

   rdp.update |= UPDATE_COMBINE;

#if 0
   FRDP("setcombine\na0=%s b0=%s c0=%s d0=%s\nAa0=%s Ab0=%s Ac0=%s Ad0=%s\na1=%s b1=%s c1=%s d1=%s\nAa1=%s Ab1=%s Ac1=%s Ad1=%s\n",
         Mode0[rdp.c_a0], Mode1[rdp.c_b0], Mode2[rdp.c_c0], Mode3[rdp.c_d0],
         Alpha0[rdp.c_Aa0], Alpha1[rdp.c_Ab0], Alpha2[rdp.c_Ac0], Alpha3[rdp.c_Ad0],
         Mode0[rdp.c_a1], Mode1[rdp.c_b1], Mode2[rdp.c_c1], Mode3[rdp.c_d1],
         Alpha0[rdp.c_Aa1], Alpha1[rdp.c_Ab1], Alpha2[rdp.c_Ac1], Alpha3[rdp.c_Ad1]);
#endif
}

static void rdp_settextureimage(uint32_t w0, uint32_t w1)
{
   //static const char *format[] = { "RGBA", "YUV", "CI", "IA", "I", "?", "?", "?" };
   //static const char *size[] = { "4bit", "8bit", "16bit", "32bit" };

   rdp.timg.format = (uint8_t)((w0 >> 21) & 0x07);
   rdp.timg.size = (uint8_t)((w0 >> 19) & 0x03);
   rdp.timg.width = (uint16_t)(1 + (w0 & 0x00000FFF));
   rdp.timg.addr = segoffset(w1);

   if (ucode5_texshiftaddr)
   {
      if (rdp.timg.format == 0)
      {
         uint16_t * t = (uint16_t*)(gfx_info.RDRAM+ucode5_texshiftaddr);
         ucode5_texshift = t[ucode5_texshiftcount^1];
         rdp.timg.addr += ucode5_texshift;
      }
      else
      {
         ucode5_texshiftaddr = 0;
         ucode5_texshift = 0;
         ucode5_texshiftcount = 0;
      }
   }
   rdp.s2dex_tex_loaded = true;
   rdp.update |= UPDATE_TEXTURE;

   if (rdp.frame_buffers[rdp.ci_count-1].status == CI_COPY_SELF && (rdp.timg.addr >= rdp.cimg) && (rdp.timg.addr < rdp.ci_end))
   {
      if (!rdp.fb_drawn)
      {
            CopyFrameBuffer(GR_BUFFER_BACKBUFFER);
         rdp.fb_drawn = true;
      }
   }


#if 0
   FRDP("settextureimage: format: %s, size: %s, width: %d, addr: %08lx\n",
         format[rdp.timg.format], size[rdp.timg.size],
         rdp.timg.width, rdp.timg.addr);
#endif
}

static void rdp_setdepthimage(uint32_t w0, uint32_t w1)
{
   rdp.zimg = segoffset(w1) & BMASK;
   rdp.zi_width = rdp.ci_width;
   //FRDP("setdepthimage - %08lx\n", rdp.zimg);
}

int SwapOK = true;

void RestoreScale(void)
{
  FRDP("Return to original scale: x = %f, y = %f\n", rdp.scale_x_bak, rdp.scale_y_bak);
  rdp.scale_x = rdp.scale_x_bak;
  rdp.scale_y = rdp.scale_y_bak;
  rdp.view_scale[0] *= rdp.scale_x;
  rdp.view_scale[1] *= rdp.scale_y;
  rdp.view_trans[0] *= rdp.scale_x;
  rdp.view_trans[1] *= rdp.scale_y;
  rdp.update |= UPDATE_VIEWPORT | UPDATE_SCISSOR;
}


static void rdp_setcolorimage(uint32_t w0, uint32_t w1)
{
   uint32_t format;

   if (fb_emulation_enabled && (rdp.num_of_ci < NUMTEXBUF))
   {
      COLOR_IMAGE *cur_fb, *prev_fb, *next_fb;
      cur_fb  = (COLOR_IMAGE*)&rdp.frame_buffers[rdp.ci_count];
      prev_fb = (COLOR_IMAGE*)&rdp.frame_buffers[rdp.ci_count?rdp.ci_count-1:0];
      next_fb = (COLOR_IMAGE*)&rdp.frame_buffers[rdp.ci_count+1];

      switch (cur_fb->status)
      {
         case CI_MAIN:
            {

               if (rdp.ci_count == 0)
               {
                  if (rdp.ci_status == CI_AUX) //for PPL
                  {
                     float sx, sy;
                     sx = rdp.scale_x;
                     sy = rdp.scale_y;
                     rdp.scale_x = 1.0f;
                     rdp.scale_y = 1.0f;
                     CopyFrameBuffer (GR_BUFFER_BACKBUFFER);
                     rdp.scale_x = sx;
                     rdp.scale_y = sy;
                  }
                  {
                     if ((rdp.num_of_ci > 1) &&
                           (next_fb->status == CI_AUX) &&
                           (next_fb->width >= cur_fb->width))
                     {
                        rdp.scale_x = 1.0f;
                        rdp.scale_y = 1.0f;
                     }
                  }
               }
               //else if (rdp.ci_status == CI_AUX && !rdp.copy_ci_index)
               // CloseTextureBuffer();

               rdp.skip_drawing = false;
            }
            break;
         case CI_COPY:
            {
               if (!rdp.motionblur || (settings.frame_buffer&fb_motionblur))
               {
                  if (cur_fb->width == rdp.ci_width)
                  {
                     {
                        if (!rdp.fb_drawn || prev_fb->status == CI_COPY_SELF)
                        {
                           CopyFrameBuffer (GR_BUFFER_BACKBUFFER);
                           rdp.fb_drawn = true;
                        }
                        memcpy(gfx_info.RDRAM+cur_fb->addr,gfx_info.RDRAM+rdp.cimg, (cur_fb->width*cur_fb->height)<<cur_fb->size>>1);
                     }
                  }
               }
               else
                  memset(gfx_info.RDRAM+cur_fb->addr, 0, cur_fb->width*cur_fb->height*rdp.ci_size);
               rdp.skip_drawing = true;
            }
            break;
         case CI_AUX_COPY:
            {
               rdp.skip_drawing = false;
                  if (!rdp.fb_drawn)
               {
                  CopyFrameBuffer (GR_BUFFER_BACKBUFFER);
                  rdp.fb_drawn = true;
               }
            }
            break;
         case CI_OLD_COPY:
            {
               if (!rdp.motionblur || (settings.frame_buffer&fb_motionblur))
               {
                  if (cur_fb->width == rdp.ci_width)
                  {
                     memcpy(gfx_info.RDRAM+cur_fb->addr,gfx_info.RDRAM+rdp.maincimg[1].addr, (cur_fb->width*cur_fb->height)<<cur_fb->size>>1);
                  }
                  //rdp.skip_drawing = true;
               }
               else
               {
                  memset(gfx_info.RDRAM+cur_fb->addr, 0, (cur_fb->width*cur_fb->height)<<rdp.ci_size>>1);
               }
            }
            break;
            /*
               else if (rdp.frame_buffers[rdp.ci_count].status == ci_main_i)
               {
            // CopyFrameBuffer ();
            rdp.scale_x = rdp.scale_x_bak;
            rdp.scale_y = rdp.scale_y_bak;
            rdp.skip_drawing = false;
            }
            */
         case CI_AUX:
            {
               if (
                     cur_fb->format != 0)
                  rdp.skip_drawing = true;
               else
               {
                  rdp.skip_drawing = false;
                  {
                     if (cur_fb->format != 0)
                        rdp.skip_drawing = true;
                     if (rdp.ci_count == 0)
                     {
                        // if (rdp.num_of_ci > 1)
                        // {
                        rdp.scale_x = 1.0f;
                        rdp.scale_y = 1.0f;
                        // }
                     }
                     else if (
                           (prev_fb->status == CI_MAIN) &&
                           (prev_fb->width == cur_fb->width)) // for Pokemon Stadium
                        CopyFrameBuffer (GR_BUFFER_BACKBUFFER);
                  }
               }
               cur_fb->status = CI_AUX;
            }
            break;
         case CI_ZIMG:
            rdp.skip_drawing = true;
            break;
         case CI_ZCOPY:
            if (settings.ucode != ucode_PerfectDark)
            {
               rdp.skip_drawing = true;
            }
            break;
         case CI_USELESS:
            rdp.skip_drawing = true;
            break;
         case CI_COPY_SELF:
            rdp.skip_drawing = false;
            break;
         default:
            rdp.skip_drawing = false;
      }

      if ((rdp.ci_count > 0) && (prev_fb->status >= CI_AUX)) //for Pokemon Stadium
      {
         if (
               prev_fb->format == 0)
            CopyFrameBuffer (GR_BUFFER_BACKBUFFER);
         else if ((settings.hacks&hack_Knockout) && prev_fb->width < 100)
            CopyFrameBuffer (GR_BUFFER_TEXTUREBUFFER_EXT);
      }
      if (cur_fb->status == CI_COPY
      )
      {
         if (!rdp.motionblur && (rdp.num_of_ci > rdp.ci_count+1) && (next_fb->status != CI_AUX))
         {
            RestoreScale();
         }
      }

      if ((cur_fb->status == CI_MAIN) && (rdp.ci_count > 0))
      {
         int to_org_res, i;
         to_org_res = true;
         for (i = rdp.ci_count + 1; i < rdp.num_of_ci; i++)
         {
            if ((rdp.frame_buffers[i].status != CI_MAIN) && (rdp.frame_buffers[i].status != CI_ZIMG) && (rdp.frame_buffers[i].status != CI_ZCOPY))
            {
               to_org_res = false;
               break;
            }
         }
         if (to_org_res)
         {
            LRDP("return to original scale\n");
            rdp.scale_x = rdp.scale_x_bak;
            rdp.scale_y = rdp.scale_y_bak;
         }
      }
      rdp.ci_status = cur_fb->status;
      rdp.ci_count++;
   }

   rdp.ocimg = rdp.cimg;
   rdp.cimg = segoffset(w1) & BMASK;
   rdp.ci_width = (w0 & 0xFFF) + 1;
   if (fb_emulation_enabled)
      rdp.ci_height = rdp.frame_buffers[rdp.ci_count-1].height;
   else if (rdp.ci_width == 32)
      rdp.ci_height = 32;
   else
      rdp.ci_height = rdp.scissor_o.lr_y;
   if (rdp.zimg == rdp.cimg)
   {
      rdp.zi_width = rdp.ci_width;
      // int zi_height = min((int)rdp.zi_width*3/4, (int)rdp.vi_height);
      // rdp.zi_words = rdp.zi_width * zi_height;
   }
   format = (w0 >> 21) & 0x7;
   rdp.ci_size = (w0 >> 19) & 0x3;
   rdp.ci_end = rdp.cimg + ((rdp.ci_width*rdp.ci_height)<<(rdp.ci_size-1));
   //FRDP("setcolorimage - %08lx, width: %d, height: %d, format: %d, size: %d\n", w1, rdp.ci_width, rdp.ci_height, format, rdp.ci_size);
   //FRDP("cimg: %08lx, ocimg: %08lx, SwapOK: %d\n", rdp.cimg, rdp.ocimg, SwapOK);

   if (format != G_IM_FMT_RGBA) //can't draw into non RGBA buffer
   {
      {
            if (format > 2)
            rdp.skip_drawing = true;
         return;
      }
   }
   else
   {
      if (!fb_emulation_enabled)
         rdp.skip_drawing = false;
   }

   CI_SET = true;
   if (settings.swapmode > 0)
   {
      int viSwapOK;

      if (rdp.zimg == rdp.cimg)
         rdp.updatescreen = 1;

      viSwapOK = ((settings.swapmode == 2) && (rdp.vi_org_reg == *gfx_info.VI_ORIGIN_REG)) ? false : true;
      if ((rdp.zimg != rdp.cimg) && (rdp.ocimg != rdp.cimg) && SwapOK && viSwapOK
            )
      {
         if (fb_emulation_enabled)
            rdp.maincimg[0] = rdp.frame_buffers[rdp.main_ci_index];
         else
            rdp.maincimg[0].addr = rdp.cimg;
         rdp.last_drawn_ci_addr = (settings.swapmode == 2) ? swapped_addr : rdp.maincimg[0].addr;
         swapped_addr = rdp.cimg;
         newSwapBuffers();
         rdp.vi_org_reg = *gfx_info.VI_ORIGIN_REG;
         SwapOK = false;
      }
   }
}

static void rsp_reserved0(uint32_t w0, uint32_t w1)
{
}

static void rsp_uc5_reserved0(uint32_t w0, uint32_t w1)
{
  ucode5_texshiftaddr = segoffset(w1);
  ucode5_texshiftcount = 0;

#ifdef EXTREME_LOGGING
  FRDP("uc5_texshift. addr: %08lx\n", ucode5_texshiftaddr);
#endif
}

static void rsp_reserved1(uint32_t w0, uint32_t w1)
{
#ifdef EXTREME_LOGGING
  LRDP("reserved1 - ignored\n");
#endif
}

static void rsp_reserved2(uint32_t w0, uint32_t w1)
{
#ifdef EXTREME_LOGGING
  LRDP("reserved2\n");
#endif
}

static void rsp_reserved3(uint32_t w0, uint32_t w1)
{
#ifdef EXTREME_LOGGING
  LRDP("reserved3 - ignored\n");
#endif
}

/******************************************************************
Function: FrameBufferRead
Purpose:  This function is called to notify the dll that the
frame buffer memory is beening read at the given address.
DLL should copy content from its render buffer to the frame buffer
in N64 RDRAM
DLL is responsible to maintain its own frame buffer memory addr list
DLL should copy 4KB block content back to RDRAM frame buffer.
Emulator should not call this function again if other memory
is read within the same 4KB range
input:    addr          rdram address
val                     val
size            1 = uint8_t, 2 = uint16_t, 4 = uint32_t
output:   none
*******************************************************************/

EXPORT void CALL FBRead(uint32_t addr)
{
  uint32_t a;
  if (cpu_fb_ignore)
    return;
  if (cpu_fb_write_called)
  {
    cpu_fb_ignore = true;
    cpu_fb_write = false;
    return;
  }
  cpu_fb_read_called = true;
  a = segoffset(addr);

#ifdef EXTREME_LOGGING
  FRDP("FBRead. addr: %08lx\n", a);
#endif

  if (!rdp.fb_drawn && (a >= rdp.cimg) && (a < rdp.ci_end))
  {
    fbreads_back++;
    //if (fbreads_back > 2) //&& (rdp.ci_width <= 320))
    {
       CopyFrameBuffer (GR_BUFFER_BACKBUFFER);
      rdp.fb_drawn = true;
    }
  }
  if (!rdp.fb_drawn_front && (a >= rdp.maincimg[1].addr) && (a < rdp.maincimg[1].addr + rdp.ci_width*rdp.ci_height*2))
  {
    fbreads_front++;
    //if (fbreads_front > 2)//&& (rdp.ci_width <= 320))
    {
      uint32_t cimg = rdp.cimg;
      rdp.cimg = rdp.maincimg[1].addr;
      if (fb_emulation_enabled)
      {
        uint32_t h;
        rdp.ci_width = rdp.maincimg[1].width;
        rdp.ci_count = 0;
        h = rdp.frame_buffers[0].height;
        rdp.frame_buffers[0].height = rdp.maincimg[1].height;
        CopyFrameBuffer(GR_BUFFER_FRONTBUFFER);
        rdp.frame_buffers[0].height = h;
      }
      else
      {
        CopyFrameBuffer(GR_BUFFER_FRONTBUFFER);
      }
      rdp.cimg = cimg;
      rdp.fb_drawn_front = true;
    }
  }
}


/******************************************************************
Function: FrameBufferWrite
Purpose:  This function is called to notify the dll that the
frame buffer has been modified by CPU at the given address.
input:    addr          rdram address
val                     val
size            1 = uint8_t, 2 = uint16_t, 4 = uint32_t
output:   none
*******************************************************************/
EXPORT void CALL FBWrite(uint32_t addr, uint32_t size)
{
  uint32_t a, shift_l, shift_r;
  if (cpu_fb_ignore)
    return;
  if (cpu_fb_read_called)
  {
    cpu_fb_ignore = true;
    cpu_fb_write = false;
    return;
  }
  cpu_fb_write_called = true;
  a = segoffset(addr);

#ifdef EXTREME_LOGGING
  FRDP("FBWrite. addr: %08lx\n", a);
#endif

  if (a < rdp.cimg || a > rdp.ci_end)
    return;
  cpu_fb_write = true;
  shift_l = (a-rdp.cimg) >> 1;
  shift_r = shift_l+2;

  d_ul_x = min(d_ul_x, shift_l%rdp.ci_width);
  d_ul_y = min(d_ul_y, shift_l/rdp.ci_width);
  d_lr_x = max(d_lr_x, shift_r%rdp.ci_width);
  d_lr_y = max(d_lr_y, shift_r/rdp.ci_width);
}


/************************************************************************
Function: FBGetFrameBufferInfo
Purpose:  This function is called by the emulator core to retrieve frame
buffer information from the video plugin in order to be able
to notify the video plugin about CPU frame buffer read/write
operations

size:
= 1           byte
= 2           word (16 bit) <-- this is N64 default depth buffer format
= 4           dword (32 bit)

when frame buffer information is not available yet, set all values
in the FrameBufferInfo structure to 0

input:    FrameBufferInfo pinfo[6]
pinfo is pointed to a FrameBufferInfo structure which to be
filled in by this function
output:   Values are return in the FrameBufferInfo structure
Plugin can return up to 6 frame buffer info
************************************************************************/
///*
EXPORT void CALL FBGetFrameBufferInfo(void *p)
{
   int i;
   FrameBufferInfo * pinfo = (FrameBufferInfo *)p;
   memset(pinfo,0,sizeof(FrameBufferInfo)*6);
   if (!(settings.frame_buffer&fb_get_info))
      return;

#ifdef EXTREME_LOGGING
   LRDP("FBGetFrameBufferInfo ()\n");
#endif
   //*
   if (fb_emulation_enabled)
   {
      int info_index;
      pinfo[0].addr   = rdp.maincimg[1].addr;
      pinfo[0].size   = rdp.maincimg[1].size;
      pinfo[0].width  = rdp.maincimg[1].width;
      pinfo[0].height = rdp.maincimg[1].height;
      info_index = 1;
      for (i = 0; i < rdp.num_of_ci && info_index < 6; i++)
      {
         COLOR_IMAGE *cur_fb = (COLOR_IMAGE*)&rdp.frame_buffers[i];
         if (cur_fb->status == CI_MAIN || cur_fb->status == CI_COPY_SELF ||
               cur_fb->status == CI_OLD_COPY)
         {
            pinfo[info_index].addr   = cur_fb->addr;
            pinfo[info_index].size   = cur_fb->size;
            pinfo[info_index].width  = cur_fb->width;
            pinfo[info_index].height = cur_fb->height;
            info_index++;
         }
      }
   }
   else
   {
      pinfo[0].addr   = rdp.maincimg[0].addr;
      pinfo[0].size   = rdp.ci_size;
      pinfo[0].width  = rdp.ci_width;
      pinfo[0].height = rdp.ci_width*3/4;
      pinfo[1].addr   = rdp.maincimg[1].addr;
      pinfo[1].size   = rdp.ci_size;
      pinfo[1].width  = rdp.ci_width;
      pinfo[1].height = rdp.ci_width*3/4;
   }
   //*/
}

#include "ucodeFB.h"

void DetectFrameBufferUsage(void)
{
   uint32_t dlist_start, a, ci, zi, ci_height;
   int i, previous_ci_was_read, all_zimg;

   LRDP("DetectFrameBufferUsage\n");

   dlist_start = *(uint32_t*)(gfx_info.DMEM+0xFF0);

   // Do nothing if dlist is empty
   if (dlist_start == 0)
      return;

   ci = rdp.cimg;
   zi = rdp.zimg;
   ci_height = rdp.frame_buffers[(rdp.ci_count > 0)?rdp.ci_count-1:0].height;
   rdp.main_ci = rdp.main_ci_end = rdp.main_ci_bg = rdp.ci_count = 0;
   rdp.main_ci_index = rdp.copy_ci_index = rdp.copy_zi_index = 0;
   rdp.zimg_end = 0;
   rdp.tmpzimg = 0;
   rdp.motionblur = false;
   rdp.main_ci_last_tex_addr = 0;
   previous_ci_was_read = rdp.read_previous_ci;
   rdp.read_previous_ci = false;
   rdp.read_whole_frame = false;
   rdp.swap_ci_index = rdp.black_ci_index = -1;
   SwapOK = true;

   // Start executing at the start of the display list
   rdp.pc_i = 0;
   rdp.pc[rdp.pc_i] = dlist_start;
   rdp.dl_count = -1;
   rdp.halt = 0;
   rdp.scale_x_bak = rdp.scale_x;
   rdp.scale_y_bak = rdp.scale_y;

   // MAIN PROCESSING LOOP
   do {

      // Get the address of the next command
      a = rdp.pc[rdp.pc_i] & BMASK;

      // Load the next command and its input
      rdp.cmd0 = ((uint32_t*)gfx_info.RDRAM)[a>>2];   // \ Current command, 64 bit
      rdp.cmd1 = ((uint32_t*)gfx_info.RDRAM)[(a>>2)+1]; // /

      // Output the address before the command

      // Go to the next instruction
      rdp.pc[rdp.pc_i] = (a+8) & BMASK;

      if ((uintptr_t)((void*)(gfx_instruction_lite[settings.ucode][rdp.cmd0>>24])))
         gfx_instruction_lite[settings.ucode][rdp.cmd0>>24](rdp.cmd0, rdp.cmd1);

      // check DL counter
      if (rdp.dl_count != -1)
      {
         rdp.dl_count --;
         if (rdp.dl_count == 0)
         {
            rdp.dl_count = -1;

            LRDP("End of DL\n");
            rdp.pc_i --;
         }
      }

   } while (!rdp.halt);
   SwapOK = true;
   if (rdp.ci_count > NUMTEXBUF) //overflow
   {
      rdp.cimg = ci;
      rdp.zimg = zi;
      rdp.num_of_ci = rdp.ci_count;
      rdp.scale_x = rdp.scale_x_bak;
      rdp.scale_y = rdp.scale_y_bak;
      return;
   }

   if (rdp.black_ci_index > 0 && rdp.black_ci_index < rdp.copy_ci_index)
      rdp.frame_buffers[rdp.black_ci_index].status = CI_MAIN;

   if (rdp.frame_buffers[rdp.ci_count-1].status == CI_UNKNOWN)
   {
      if (rdp.ci_count > 1)
         rdp.frame_buffers[rdp.ci_count-1].status = CI_AUX;
      else
         rdp.frame_buffers[rdp.ci_count-1].status = CI_MAIN;
   }

   if ((rdp.frame_buffers[rdp.ci_count-1].status == CI_AUX) &&
         (rdp.frame_buffers[rdp.main_ci_index].width < 320) &&
         (rdp.frame_buffers[rdp.ci_count-1].width > rdp.frame_buffers[rdp.main_ci_index].width))
   {
      for (i = 0; i < rdp.ci_count; i++)
      {
         if (rdp.frame_buffers[i].status == CI_MAIN)
            rdp.frame_buffers[i].status = CI_AUX;
         else if (rdp.frame_buffers[i].addr == rdp.frame_buffers[rdp.ci_count-1].addr)
            rdp.frame_buffers[i].status = CI_MAIN;
         //                        FRDP("rdp.frame_buffers[%d].status = %d\n", i, rdp.frame_buffers[i].status);
      }
      rdp.main_ci_index = rdp.ci_count-1;
   }

   all_zimg = true;
   for (i = 0; i < rdp.ci_count; i++)
   {
      if (rdp.frame_buffers[i].status != CI_ZIMG)
      {
         all_zimg = false;
         break;
      }
   }
   if (all_zimg)
   {
      for (i = 0; i < rdp.ci_count; i++)
         rdp.frame_buffers[i].status = CI_MAIN;
   }

#ifndef NDEBUG
   LRDP("detect fb final results: \n");
   for (i = 0; i < rdp.ci_count; i++)
   {
      FRDP("rdp.frame_buffers[%d].status = %s, addr: %08lx, height: %d\n", i, CIStatus[rdp.frame_buffers[i].status], rdp.frame_buffers[i].addr, rdp.frame_buffers[i].height);
   }
#endif

   rdp.cimg = ci;
   rdp.zimg = zi;
   rdp.num_of_ci = rdp.ci_count;
   if (rdp.read_previous_ci && previous_ci_was_read)
   {
      if (!fb_hwfbe_enabled || !rdp.copy_ci_index)
         rdp.motionblur = true;
   }
   if (rdp.motionblur || fb_hwfbe_enabled || (rdp.frame_buffers[rdp.copy_ci_index].status == CI_AUX_COPY))
   {
      rdp.scale_x = rdp.scale_x_bak;
      rdp.scale_y = rdp.scale_y_bak;
   }

   if ((rdp.read_previous_ci || previous_ci_was_read) && !rdp.copy_ci_index)
      rdp.read_whole_frame = true;
   if (rdp.read_whole_frame)
   {
      {
         if (rdp.motionblur)
         {
            if (settings.frame_buffer&fb_motionblur)
               CopyFrameBuffer (GR_BUFFER_BACKBUFFER);
            else
               memset(gfx_info.RDRAM+rdp.cimg, 0, rdp.ci_width*rdp.ci_height*rdp.ci_size);
         }
         else //if (ci_width == rdp.frame_buffers[rdp.main_ci_index].width)
         {
            if (rdp.maincimg[0].height > 65) //for 1080
            {
               uint32_t h;
               rdp.cimg = rdp.maincimg[0].addr;
               rdp.ci_width = rdp.maincimg[0].width;
               rdp.ci_count = 0;
               h = rdp.frame_buffers[0].height;
               rdp.frame_buffers[0].height = rdp.maincimg[0].height;
               CopyFrameBuffer (GR_BUFFER_BACKBUFFER);
               rdp.frame_buffers[0].height = h;
            }
            else //conker
            {
               CopyFrameBuffer (GR_BUFFER_BACKBUFFER);
            }
         }
      }
   }

   rdp.ci_count = 0;
   rdp.maincimg[0] = rdp.frame_buffers[rdp.main_ci_index];
   //rdp.scale_x = rdp.scale_x_bak;
   //rdp.scale_y = rdp.scale_y_bak;
  
   //LRDP("DetectFrameBufferUsage End\n");
}

/*******************************************
 *          ProcessRDPList                 *
 *******************************************
 *    based on sources of ziggy's z64      *
 *******************************************/

static uint32_t rdp_cmd_ptr = 0;
static uint32_t rdp_cmd_cur = 0;
static uint32_t rdp_cmd_data[0x1000];

#define XSCALE(x) ((float)(x)/(1<<18))
#define YSCALE(y) ((float)(y)/(1<<2))
#define ZSCALE(z) ((rdp.zsrc == 1) ? (float)(rdp.prim_depth) : (float)((uint32_t)(z))/0xffff0000)
  //#define WSCALE(w) ((rdp.othermode_h & RDP_PERSP_TEX_ENABLE) ? (float(uint32_t(w) + 0x10000)/0xffff0000) : 1.0f)
  //#define WSCALE(w) ((rdp.othermode_h & RDP_PERSP_TEX_ENABLE) ? 4294901760.0/(w + 65536) : 1.0f)
#define WSCALE(w) ((rdp.othermode_h & RDP_PERSP_TEX_ENABLE) ? 65536.0f/ (float)((w+ 0xffff)>>16) : 1.0f)
#define CSCALE(c) (((c)>0x3ff0000? 0x3ff0000:((c)<0? 0 : (c)))>>18)
#define _PERSP(w) ( w )
#define PERSP(s, w) ( ((int64_t)(s) << 20) / (_PERSP(w)? _PERSP(w):1) )
#define SSCALE(s, _w) ((rdp.othermode_h & RDP_PERSP_TEX_ENABLE) ? (float)(PERSP(s, _w))/(1 << 10) : (float)(s)/(1<<21))
#define TSCALE(s, w)  ((rdp.othermode_h & RDP_PERSP_TEX_ENABLE) ? (float)(PERSP(s, w))/(1 << 10) : (float)(s)/(1<<21))

static void lle_triangle(uint32_t w1, uint32_t w2, int shade, int texture, int zbuffer, uint32_t *rdp_cmd)
{
   int j;
   int xleft, xright, xleft_inc, xright_inc;
   VERTEX vtxbuf[12], *vtx;

   int32_t nbVtxs;

   int r = 0xff;
   int g = 0xff;
   int b = 0xff;
   int a = 0xff;
   int z = 0xffff0000;
   int s = 0;
   int t = 0;
   int w = 0x30000;

   int drdx = 0, dgdx = 0, dbdx = 0, dadx = 0, dzdx = 0, dsdx = 0, dtdx = 0, dwdx = 0;
   int drde = 0, dgde = 0, dbde = 0, dade = 0, dzde = 0, dsde = 0, dtde = 0, dwde = 0;
   int flip = (w1 & 0x800000) ? 1 : 0;
   uint32_t * shade_base = rdp_cmd + 8;
   uint32_t * texture_base = rdp_cmd + 8;
   uint32_t * zbuffer_base = rdp_cmd + 8;

   uint32_t w3 = rdp_cmd[2];
   uint32_t w4 = rdp_cmd[3];
   uint32_t w5 = rdp_cmd[4];
   uint32_t w6 = rdp_cmd[5];
   uint32_t w7 = rdp_cmd[6];
   uint32_t w8 = rdp_cmd[7];

   /* triangle edge y-coordinates */
   int32_t yl = (w1 & 0x3fff);
   int32_t ym = ((w2 >> 16) & 0x3fff);
   int32_t yh = ((w2 >> 0) & 0x3fff);

   /* triangle edge x-coordinates */
   int32_t xl = (int32_t)(w3);
   int32_t xh = (int32_t)(w5);
   int32_t xm = (int32_t)(w7);

   /* triangle edge inverse-slopes */
   int32_t dxldy = (int32_t)(w4);
   int32_t dxhdy = (int32_t)(w6);
   int32_t dxmdy = (int32_t)(w8);

   rdp.cur_tile = (w1 >> 16) & 0x7;

   if (shade)
   {
      texture_base += 16;
      zbuffer_base += 16;
   }
   if (texture)
   {
      zbuffer_base += 16;
   }

   if (yl & (0x800<<2)) yl |= 0xfffff000<<2;
   if (ym & (0x800<<2)) ym |= 0xfffff000<<2;
   if (yh & (0x800<<2)) yh |= 0xfffff000<<2;

   yh &= ~3;


   if (shade)
   {
      r = (shade_base[0] & 0xffff0000) | ((shade_base[+4 ] >> 16) & 0x0000ffff);
      g = ((shade_base[0 ] << 16) & 0xffff0000) | (shade_base[4 ] & 0x0000ffff);
      b = (shade_base[1 ] & 0xffff0000) | ((shade_base[5 ] >> 16) & 0x0000ffff);
      a = ((shade_base[1 ] << 16) & 0xffff0000) | (shade_base[5 ] & 0x0000ffff);
      drdx = (shade_base[2 ] & 0xffff0000) | ((shade_base[6 ] >> 16) & 0x0000ffff);
      dgdx = ((shade_base[2 ] << 16) & 0xffff0000) | (shade_base[6 ] & 0x0000ffff);
      dbdx = (shade_base[3 ] & 0xffff0000) | ((shade_base[7 ] >> 16) & 0x0000ffff);
      dadx = ((shade_base[3 ] << 16) & 0xffff0000) | (shade_base[7 ] & 0x0000ffff);
      drde = (shade_base[8 ] & 0xffff0000) | ((shade_base[12] >> 16) & 0x0000ffff);
      dgde = ((shade_base[8 ] << 16) & 0xffff0000) | (shade_base[12] & 0x0000ffff);
      dbde = (shade_base[9 ] & 0xffff0000) | ((shade_base[13] >> 16) & 0x0000ffff);
      dade = ((shade_base[9 ] << 16) & 0xffff0000) | (shade_base[13] & 0x0000ffff);
   }
   if (texture)
   {
      s = (texture_base[0 ] & 0xffff0000) | ((texture_base[4 ] >> 16) & 0x0000ffff);
      t = ((texture_base[0 ] << 16) & 0xffff0000) | (texture_base[4 ] & 0x0000ffff);
      w = (texture_base[1 ] & 0xffff0000) | ((texture_base[5 ] >> 16) & 0x0000ffff);
      // w = abs(w);
      dsdx = (texture_base[2 ] & 0xffff0000) | ((texture_base[6 ] >> 16) & 0x0000ffff);
      dtdx = ((texture_base[2 ] << 16) & 0xffff0000) | (texture_base[6 ] & 0x0000ffff);
      dwdx = (texture_base[3 ] & 0xffff0000) | ((texture_base[7 ] >> 16) & 0x0000ffff);
      dsde = (texture_base[8 ] & 0xffff0000) | ((texture_base[12] >> 16) & 0x0000ffff);
      dtde = ((texture_base[8 ] << 16) & 0xffff0000) | (texture_base[12] & 0x0000ffff);
      dwde = (texture_base[9 ] & 0xffff0000) | ((texture_base[13] >> 16) & 0x0000ffff);
   }
   if (zbuffer)
   {
      z = zbuffer_base[0];
      dzdx = zbuffer_base[1];
      dzde = zbuffer_base[2];
   }

   xh <<= 2; xm <<= 2; xl <<= 2;
   r <<= 2; g <<= 2; b <<= 2; a <<= 2;
   dsde >>= 2; dtde >>= 2; dsdx >>= 2; dtdx >>= 2;
   dzdx >>= 2; dzde >>= 2;
   dwdx >>= 2; dwde >>= 2;

   nbVtxs = 0;
   vtx = (VERTEX*)&vtxbuf[nbVtxs++];

   xleft = xm;
   xright = xh;
   xleft_inc = dxmdy;
   xright_inc = dxhdy;

   while (yh<ym &&
         !((!flip && xleft < xright+0x10000) ||
            (flip && xleft > xright-0x10000))) {
      xleft += xleft_inc;
      xright += xright_inc;
      s += dsde; t += dtde; w += dwde;
      r += drde; g += dgde; b += dbde; a += dade;
      z += dzde;
      yh++;
   }

   j = ym-yh;
   if (j > 0)
   {
      int dx = (xleft-xright)>>16;
      if ((!flip && xleft < xright) ||
            (flip/* && xleft > xright*/))
      {
         if (shade) {
            vtx->r = CSCALE(r+drdx*dx);
            vtx->g = CSCALE(g+dgdx*dx);
            vtx->b = CSCALE(b+dbdx*dx);
            vtx->a = CSCALE(a+dadx*dx);
         }
         if (texture) {
            vtx->ou = SSCALE(s+dsdx*dx, w+dwdx*dx);
            vtx->ov = TSCALE(t+dtdx*dx, w+dwdx*dx);
         }
         vtx->x = XSCALE(xleft);
         vtx->y = YSCALE(yh);
         vtx->z = ZSCALE(z+dzdx*dx);
         vtx->w = WSCALE(w+dwdx*dx);
         vtx = &vtxbuf[nbVtxs++];
      }
      if ((!flip/* && xleft < xright*/) ||
            (flip && xleft > xright))
      {
         if (shade) {
            vtx->r = CSCALE(r);
            vtx->g = CSCALE(g);
            vtx->b = CSCALE(b);
            vtx->a = CSCALE(a);
         }
         if (texture) {
            vtx->ou = SSCALE(s, w);
            vtx->ov = TSCALE(t, w);
         }
         vtx->x = XSCALE(xright);
         vtx->y = YSCALE(yh);
         vtx->z = ZSCALE(z);
         vtx->w = WSCALE(w);
         vtx = &vtxbuf[nbVtxs++];
      }
      xleft += xleft_inc*j; xright += xright_inc*j;
      s += dsde*j; t += dtde*j;
      if (w + dwde*j) w += dwde*j;
      else w += dwde*(j-1);
      r += drde*j; g += dgde*j; b += dbde*j; a += dade*j;
      z += dzde*j;
      // render ...
   }

   if (xl != xh)
      xleft = xl;

   //if (yl-ym > 0)
   {
      int dx = (xleft-xright)>>16;
      if ((!flip && xleft <= xright) ||
            (flip/* && xleft >= xright*/))
      {
         if (shade) {
            vtx->r = CSCALE(r+drdx*dx);
            vtx->g = CSCALE(g+dgdx*dx);
            vtx->b = CSCALE(b+dbdx*dx);
            vtx->a = CSCALE(a+dadx*dx);
         }
         if (texture) {
            vtx->ou = SSCALE(s+dsdx*dx, w+dwdx*dx);
            vtx->ov = TSCALE(t+dtdx*dx, w+dwdx*dx);
         }
         vtx->x = XSCALE(xleft);
         vtx->y = YSCALE(ym);
         vtx->z = ZSCALE(z+dzdx*dx);
         vtx->w = WSCALE(w+dwdx*dx);
         vtx = &vtxbuf[nbVtxs++];
      }
      if ((!flip/* && xleft <= xright*/) ||
            (flip && xleft >= xright))
      {
         if (shade) {
            vtx->r = CSCALE(r);
            vtx->g = CSCALE(g);
            vtx->b = CSCALE(b);
            vtx->a = CSCALE(a);
         }
         if (texture) {
            vtx->ou = SSCALE(s, w);
            vtx->ov = TSCALE(t, w);
         }
         vtx->x = XSCALE(xright);
         vtx->y = YSCALE(ym);
         vtx->z = ZSCALE(z);
         vtx->w = WSCALE(w);
         vtx = &vtxbuf[nbVtxs++];
      }
   }
   xleft_inc = dxldy;
   xright_inc = dxhdy;

   j = yl-ym;
   //j--; // ?
   xleft += xleft_inc*j; xright += xright_inc*j;
   s += dsde*j; t += dtde*j; w += dwde*j;
   r += drde*j; g += dgde*j; b += dbde*j; a += dade*j;
   z += dzde*j;

   while (yl>ym &&
         !((!flip && xleft < xright+0x10000) ||
            (flip && xleft > xright-0x10000))) {
      xleft -= xleft_inc; xright -= xright_inc;
      s -= dsde; t -= dtde; w -= dwde;
      r -= drde; g -= dgde; b -= dbde; a -= dade;
      z -= dzde;
      j--;
      yl--;
   }

   // render ...
   if (j >= 0) {
      int dx = (xleft-xright)>>16;
      if ((!flip && xleft <= xright) ||
            (flip/* && xleft >= xright*/))
      {
         if (shade) {
            vtx->r = CSCALE(r+drdx*dx);
            vtx->g = CSCALE(g+dgdx*dx);
            vtx->b = CSCALE(b+dbdx*dx);
            vtx->a = CSCALE(a+dadx*dx);
         }
         if (texture) {
            vtx->ou = SSCALE(s+dsdx*dx, w+dwdx*dx);
            vtx->ov = TSCALE(t+dtdx*dx, w+dwdx*dx);
         }
         vtx->x = XSCALE(xleft);
         vtx->y = YSCALE(yl);
         vtx->z = ZSCALE(z+dzdx*dx);
         vtx->w = WSCALE(w+dwdx*dx);
         vtx = &vtxbuf[nbVtxs++];
      }
      if ((!flip/* && xleft <= xright*/) ||
            (flip && xleft >= xright))
      {
         if (shade) {
            vtx->r = CSCALE(r);
            vtx->g = CSCALE(g);
            vtx->b = CSCALE(b);
            vtx->a = CSCALE(a);
         }
         if (texture) {
            vtx->ou = SSCALE(s, w);
            vtx->ov = TSCALE(t, w);
         }
         vtx->x = XSCALE(xright);
         vtx->y = YSCALE(yl);
         vtx->z = ZSCALE(z);
         vtx->w = WSCALE(w);
         vtx = &vtxbuf[nbVtxs++];
      }
   }

   //if (fullscreen)
   {
      int k;
      update ();
      for (k = 0; k < nbVtxs-1; k++)
      {
         VERTEX * v = (VERTEX*)&vtxbuf[k];
         v->x = v->x * rdp.scale_x + rdp.offset_x;
         v->y = v->y * rdp.scale_y + rdp.offset_y;
         // v->z = 1.0f;///v->w;
         v->q = 1.0f/v->w;
         v->u1 = v->u0 = v->ou;
         v->v1 = v->v0 = v->ov;
         if (rdp.tex >= 1 && rdp.cur_cache[0])
         {
            if (rdp.tiles[rdp.cur_tile].shift_s)
            {
               if (rdp.tiles[rdp.cur_tile].shift_s > 10)
                  v->u0 *= (float)(1 << (16 - rdp.tiles[rdp.cur_tile].shift_s));
               else
                  v->u0 /= (float)(1 << rdp.tiles[rdp.cur_tile].shift_s);
            }
            if (rdp.tiles[rdp.cur_tile].shift_t)
            {
               if (rdp.tiles[rdp.cur_tile].shift_t > 10)
                  v->v0 *= (float)(1 << (16 - rdp.tiles[rdp.cur_tile].shift_t));
               else
                  v->v0 /= (float)(1 << rdp.tiles[rdp.cur_tile].shift_t);
            }

            v->u0 -= rdp.tiles[rdp.cur_tile].f_ul_s;
            v->v0 -= rdp.tiles[rdp.cur_tile].f_ul_t;
            v->u0 = rdp.cur_cache[0]->c_off + rdp.cur_cache[0]->c_scl_x * v->u0;
            v->v0 = rdp.cur_cache[0]->c_off + rdp.cur_cache[0]->c_scl_y * v->v0;
            v->u0 /= v->w;
            v->v0 /= v->w;
         }

         if (rdp.tex >= 2 && rdp.cur_cache[1])
         {
            if (rdp.tiles[rdp.cur_tile+1].shift_s)
            {
               if (rdp.tiles[rdp.cur_tile+1].shift_s > 10)
                  v->u1 *= (float)(1 << (16 - rdp.tiles[rdp.cur_tile+1].shift_s));
               else
                  v->u1 /= (float)(1 << rdp.tiles[rdp.cur_tile+1].shift_s);
            }
            if (rdp.tiles[rdp.cur_tile+1].shift_t)
            {
               if (rdp.tiles[rdp.cur_tile+1].shift_t > 10)
                  v->v1 *= (float)(1 << (16 - rdp.tiles[rdp.cur_tile+1].shift_t));
               else
                  v->v1 /= (float)(1 << rdp.tiles[rdp.cur_tile+1].shift_t);
            }

            v->u1 -= rdp.tiles[rdp.cur_tile+1].f_ul_s;
            v->v1 -= rdp.tiles[rdp.cur_tile+1].f_ul_t;
            v->u1 = rdp.cur_cache[1]->c_off + rdp.cur_cache[1]->c_scl_x * v->u1;
            v->v1 = rdp.cur_cache[1]->c_off + rdp.cur_cache[1]->c_scl_y * v->v1;
            v->u1 /= v->w;
            v->v1 /= v->w;
         }
         apply_shade_mods (v);
      }
      grCullMode (GR_CULL_DISABLE);
      ConvertCoordsConvert (vtxbuf, nbVtxs);
      grDrawVertexArrayContiguous (GR_TRIANGLE_STRIP, nbVtxs-1, vtxbuf);
   }
}

#define rdp_triangle(w0, w1, shade, texture, zbuffer, wptr) lle_triangle(w0, w1, shade, texture, zbuffer, (wptr))

static void rdp_trifill(uint32_t w0, uint32_t w1)
{
   rdp_triangle(w0, w1, 0, 0, 0, rdp_cmd_data + rdp_cmd_cur);
#ifdef EXTREME_LOGGING
  LRDP("trifill\n");
#endif
}

static void rdp_trishade(uint32_t w0, uint32_t w1)
{
   rdp_triangle(w0, w1, 1, 0, 0, rdp_cmd_data + rdp_cmd_cur);
#ifdef EXTREME_LOGGING
  LRDP("trishade\n");
#endif
}

static void rdp_tritxtr(uint32_t w0, uint32_t w1)
{
   rdp_triangle(w0, w1, 0, 1, 0, rdp_cmd_data + rdp_cmd_cur);
#ifdef EXTREME_LOGGING
  LRDP("tritxtr\n");
#endif
}

static void rdp_trishadetxtr(uint32_t w0, uint32_t w1)
{
   rdp_triangle(w0, w1, 1, 1, 0, rdp_cmd_data + rdp_cmd_cur);
#ifdef EXTREME_LOGGING
  LRDP("trishadetxtr\n");
#endif
}

static void rdp_trifillz(uint32_t w0, uint32_t w1)
{
   rdp_triangle(w0, w1, 0, 0, 1, rdp_cmd_data + rdp_cmd_cur);
#ifdef EXTREME_LOGGING
  LRDP("trifillz\n");
#endif
}

static void rdp_trishadez(uint32_t w0, uint32_t w1)
{
   rdp_triangle(w0, w1, 1, 0, 1, rdp_cmd_data + rdp_cmd_cur);
#ifdef EXTREME_LOGGING
  LRDP("trishadez\n");
#endif
}

static void rdp_tritxtrz(uint32_t w0, uint32_t w1)
{
   rdp_triangle(w0, w1, 0, 1, 1, rdp_cmd_data + rdp_cmd_cur);
#ifdef EXTREME_LOGGING
   LRDP("tritxtrz\n");
#endif
}

static void rdp_trishadetxtrz(uint32_t w0, uint32_t w1)
{
   rdp_triangle(w0, w1, 1, 1, 1, rdp_cmd_data + rdp_cmd_cur);
#ifdef EXTREME_LOGGING
   LRDP("trishadetxtrz\n");
#endif
}


static rdp_instr rdp_command_table[64] =
{
   /* 0x00 */
   spnoop,             undef,                  undef,                  undef,
   undef,              undef,                  undef,                  undef,
   rdp_trifill,        rdp_trifillz,           rdp_tritxtr,            rdp_tritxtrz,
   rdp_trishade,       rdp_trishadez,          rdp_trishadetxtr,       rdp_trishadetxtrz,
   /* 0x10 */
   undef,              undef,                  undef,                  undef,
   undef,              undef,                  undef,                  undef,
   undef,              undef,                  undef,                  undef,
   undef,              undef,                  undef,                  undef,
   /* 0x20 */
   undef,              undef,                  undef,                  undef,
   rdp_texrect,        rdp_texrect,            rdp_loadsync,           rdp_pipesync,
   rdp_tilesync,       rdp_fullsync,           rdp_setkeygb,           rdp_setkeyr,
   rdp_setconvert,     rdp_setscissor,         rdp_setprimdepth,       rdp_setothermode,
   /* 0x30 */
   rdp_loadtlut,           undef,                  rdp_settilesize,        rdp_loadblock,
   rdp_loadtile,           rdp_settile,            rdp_fillrect,           rdp_setfillcolor,
   rdp_setfogcolor,        rdp_setblendcolor,      rdp_setprimcolor,       rdp_setenvcolor,
   rdp_setcombine,         rdp_settextureimage,    rdp_setdepthimage,      rdp_setcolorimage
};

static const uint32_t rdp_command_length[64] =
{
   8,                      // 0x00, No Op
   8,                      // 0x01, ???
   8,                      // 0x02, ???
   8,                      // 0x03, ???
   8,                      // 0x04, ???
   8,                      // 0x05, ???
   8,                      // 0x06, ???
   8,                      // 0x07, ???
   32,                     // 0x08, Non-Shaded Triangle
   32+16,                  // 0x09, Non-Shaded, Z-Buffered Triangle
   32+64,                  // 0x0a, Textured Triangle
   32+64+16,               // 0x0b, Textured, Z-Buffered Triangle
   32+64,                  // 0x0c, Shaded Triangle
   32+64+16,               // 0x0d, Shaded, Z-Buffered Triangle
   32+64+64,               // 0x0e, Shaded+Textured Triangle
   32+64+64+16,            // 0x0f, Shaded+Textured, Z-Buffered Triangle
   8,                      // 0x10, ???
   8,                      // 0x11, ???
   8,                      // 0x12, ???
   8,                      // 0x13, ???
   8,                      // 0x14, ???
   8,                      // 0x15, ???
   8,                      // 0x16, ???
   8,                      // 0x17, ???
   8,                      // 0x18, ???
   8,                      // 0x19, ???
   8,                      // 0x1a, ???
   8,                      // 0x1b, ???
   8,                      // 0x1c, ???
   8,                      // 0x1d, ???
   8,                      // 0x1e, ???
   8,                      // 0x1f, ???
   8,                      // 0x20, ???
   8,                      // 0x21, ???
   8,                      // 0x22, ???
   8,                      // 0x23, ???
   16,                     // 0x24, Texture_Rectangle
   16,                     // 0x25, Texture_Rectangle_Flip
   8,                      // 0x26, Sync_Load
   8,                      // 0x27, Sync_Pipe
   8,                      // 0x28, Sync_Tile
   8,                      // 0x29, Sync_Full
   8,                      // 0x2a, Set_Key_GB
   8,                      // 0x2b, Set_Key_R
   8,                      // 0x2c, Set_Convert
   8,                      // 0x2d, Set_Scissor
   8,                      // 0x2e, Set_Prim_Depth
   8,                      // 0x2f, Set_Other_Modes
   8,                      // 0x30, Load_TLUT
   8,                      // 0x31, ???
   8,                      // 0x32, Set_Tile_Size
   8,                      // 0x33, Load_Block
   8,                      // 0x34, Load_Tile
   8,                      // 0x35, Set_Tile
   8,                      // 0x36, Fill_Rectangle
   8,                      // 0x37, Set_Fill_Color
   8,                      // 0x38, Set_Fog_Color
   8,                      // 0x39, Set_Blend_Color
   8,                      // 0x3a, Set_Prim_Color
   8,                      // 0x3b, Set_Env_Color
   8,                      // 0x3c, Set_Combine
   8,                      // 0x3d, Set_Texture_Image
   8,                      // 0x3e, Set_Mask_Image
   8                       // 0x3f, Set_Color_Image
};

static INLINE uint32_t READ_RDP_DATA(uint32_t address)
{
   if ((*(uint32_t*)gfx_info.DPC_STATUS_REG) & 0x1) /* XBUS_DMEM_DMA enabled */
      return ((uint32_t*)gfx_info.DMEM)[(address & 0xfff) >> 2];
   return ((uint32_t*)gfx_info.RDRAM)[address >> 2];
}

static void rdphalf_1(uint32_t w0, uint32_t w1)
{
   uint32_t cmd = rdp.cmd1 >> 24;
   if (cmd >= G_TRI_FILL && cmd <= G_TRI_SHADE_TXTR_ZBUFF) //triangle command
   {
      uint32_t a;
      LRDP("rdphalf_1 - lle triangle\n");
      rdp_cmd_ptr = 0;
      rdp_cmd_cur = 0;

      do
      {
         rdp_cmd_data[rdp_cmd_ptr++] = rdp.cmd1;
         // check DL counter
         if (rdp.dl_count != -1)
         {
            rdp.dl_count --;
            if (rdp.dl_count == 0)
            {
               rdp.dl_count = -1;

               LRDP("End of DL\n");
               rdp.pc_i --;
            }
         }

         // Get the address of the next command
         a = rdp.pc[rdp.pc_i] & BMASK;

         // Load the next command and its input
         rdp.cmd0 = ((uint32_t*)gfx_info.RDRAM)[a>>2];   // \ Current command, 64 bit
         rdp.cmd1 = ((uint32_t*)gfx_info.RDRAM)[(a>>2)+1]; // /

         // Go to the next instruction
         rdp.pc[rdp.pc_i] = (a+8) & BMASK;

      }while ((rdp.cmd0 >> 24) != 0xb3);
      rdp_cmd_data[rdp_cmd_ptr++] = rdp.cmd1;
      cmd = (rdp_cmd_data[rdp_cmd_cur] >> 24) & 0x3f;
      rdp.cmd0 = rdp_cmd_data[rdp_cmd_cur+0];
      rdp.cmd1 = rdp_cmd_data[rdp_cmd_cur+1];
      /*
         uint32_t cmd3 = ((uint32_t*)gfx_info.RDRAM)[(a>>2)+2];
         if ((cmd3>>24) == 0xb4)
         rglSingleTriangle = true;
         else
         rglSingleTriangle = false;
         */
      rdp_command_table[cmd](rdp.cmd0, rdp.cmd1);
   }
   //LRDP("rdphalf_1\n");
}

static void rdphalf_2(uint32_t w0, uint32_t w1)
{
   RDP_E("rdphalf_2 - IGNORED\n");
   LRDP("rdphalf_2 - IGNORED\n");
}

static void rdphalf_cont(uint32_t w0, uint32_t w1)
{
   RDP_E("rdphalf_cont - IGNORED\n");
   LRDP("rdphalf_cont - IGNORED\n");
}

/******************************************************************
Function: ProcessRDPList
Purpose:  This function is called when there is a Dlist to be
processed. (Low level GFX list)
input:    none
output:   none
*******************************************************************/
EXPORT void CALL ProcessRDPList(void)
{
   int32_t length;
   uint32_t i;
   uint32_t cmd, cmd_length;

   rdp_cmd_ptr = 0;
   rdp_cmd_cur = 0;
   length = (*(uint32_t*)gfx_info.DPC_END_REG) - (*(uint32_t*)gfx_info.DPC_CURRENT_REG);

   if ((*(uint32_t*)gfx_info.DPC_END_REG) <= (*(uint32_t*)gfx_info.DPC_CURRENT_REG))
      return;

   if (length < 0)
   {
      (*(uint32_t*)gfx_info.DPC_CURRENT_REG) = (*(uint32_t*)gfx_info.DPC_END_REG);
      return;
   }

   // load command data
   for (i=0; i < length; i += 4)
      rdp_cmd_data[rdp_cmd_ptr++] = READ_RDP_DATA(((*(uint32_t*)gfx_info.DPC_CURRENT_REG) & 0x1fffffff) + i);

   (*(uint32_t*)gfx_info.DPC_CURRENT_REG) = (*(uint32_t*)gfx_info.DPC_END_REG);

   cmd = (rdp_cmd_data[0] >> 24) & 0x3f;
   cmd_length = (rdp_cmd_ptr + 1) * 4;

   // check if more data is needed
   if (cmd_length < rdp_command_length[cmd])
      return;

   rdp.LLE = true;

   while (rdp_cmd_cur < rdp_cmd_ptr)
   {
      uint32_t w0, w1;
      cmd = (rdp_cmd_data[rdp_cmd_cur] >> 24) & 0x3f;

      if (((rdp_cmd_ptr-rdp_cmd_cur) * 4) < rdp_command_length[cmd])
         return;

      // execute the command
      rdp.cmd0 = rdp_cmd_data[rdp_cmd_cur+0];
      rdp.cmd1 = rdp_cmd_data[rdp_cmd_cur+1];
      rdp.cmd2 = rdp_cmd_data[rdp_cmd_cur+2];
      rdp.cmd3 = rdp_cmd_data[rdp_cmd_cur+3];

      w0 = rdp.cmd0;
      w1 = rdp.cmd1;

      //printf("cmd: %d (w0: %d, w1: %d)\n", cmd, w0, w1);
      rdp_command_table[cmd](w0, w1);

      rdp_cmd_cur += rdp_command_length[cmd] / 4;
   }

   rdp.LLE = false;
   (*(uint32_t*)gfx_info.DPC_START_REG) = (*(uint32_t*)gfx_info.DPC_END_REG);
   (*(uint32_t*)gfx_info.DPC_STATUS_REG) &= ~0x0002;
   rdp_cmd_cur = 0;
   rdp_cmd_ptr = 0;
}
