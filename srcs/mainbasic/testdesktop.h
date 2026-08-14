#ifndef TESTDESKTOP
#define TESTDESKTOP

#include <QString>
#include "vector"
#include "mysqlitepatients.h"

#include "opencv2/core/core_c.h"

typedef unsigned short __u16;
typedef unsigned int __u32;

#define FBIOBLANK		0x4611		/* arg: 0 or vesa level + 1 */

/* VESA Blanking Levels */
#define VESA_NO_BLANKING        0
#define VESA_VSYNC_SUSPEND      1
#define VESA_HSYNC_SUSPEND      2
#define VESA_POWERDOWN          3

enum {
    /* screen: unblanked, hsync: on,  vsync: on */
    FB_BLANK_UNBLANK       = VESA_NO_BLANKING,

    /* screen: blanked,   hsync: on,  vsync: on */
    FB_BLANK_NORMAL        = VESA_NO_BLANKING + 1,

    /* screen: blanked,   hsync: on,  vsync: off */
    FB_BLANK_VSYNC_SUSPEND = VESA_VSYNC_SUSPEND + 1,

    /* screen: blanked,   hsync: off, vsync: on */
    FB_BLANK_HSYNC_SUSPEND = VESA_HSYNC_SUSPEND + 1,

    /* screen: blanked,   hsync: off, vsync: off */
    FB_BLANK_POWERDOWN     = VESA_POWERDOWN + 1
};


/* Interpretation of offset for color fields: All offsets are from the right,
 * inside a "pixel" value, which is exactly 'bits_per_pixel' wide (means: you
 * can use the offset as right argument to <<). A pixel afterwards is a bit
 * stream and is written to video memory as that unmodified.
 *
 * For pseudocolor: offset and length should be the same for all color
 * components. Offset specifies the position of the least significant bit
 * of the pallette index in a pixel value. Length indicates the number
 * of available palette entries (i.e. # of entries = 1 << length).
 */
struct fb_bitfield {
    __u32 offset;			/* beginning of bitfield	*/
    __u32 length;			/* length of bitfield		*/
    __u32 msb_right;		/* != 0 : Most significant bit is */
                    /* right */
};

struct fb_var_screeninfo {
    __u32 xres;			/* visible resolution		*/
    __u32 yres;
    __u32 xres_virtual;		/* virtual resolution		*/
    __u32 yres_virtual;
    __u32 xoffset;			/* offset from virtual to visible */
    __u32 yoffset;			/* resolution			*/

    __u32 bits_per_pixel;		/* guess what			*/
    __u32 grayscale;		/* 0 = color, 1 = grayscale,	*/
                    /* >1 = FOURCC			*/
    struct fb_bitfield red;		/* bitfield in fb mem if true color, */
    struct fb_bitfield green;	/* else only length is significant */
    struct fb_bitfield blue;
    struct fb_bitfield transp;	/* transparency			*/

    __u32 nonstd;			/* != 0 Non standard pixel format */

    __u32 activate;			/* see FB_ACTIVATE_*		*/

    __u32 height;			/* height of picture in mm    */
    __u32 width;			/* width of picture in mm     */

    __u32 accel_flags;		/* (OBSOLETE) see fb_info.flags */

    /* Timing: All values in pixclocks, except pixclock (of course) */
    __u32 pixclock;			/* pixel clock in ps (pico seconds) */
    __u32 left_margin;		/* time from sync to picture	*/
    __u32 right_margin;		/* time from picture to sync	*/
    __u32 upper_margin;		/* time from sync to picture	*/
    __u32 lower_margin;
    __u32 hsync_len;		/* length of horizontal sync	*/
    __u32 vsync_len;		/* length of vertical sync	*/
    __u32 sync;			/* see FB_SYNC_*		*/
    __u32 vmode;			/* see FB_VMODE_*		*/
    __u32 rotate;			/* angle we rotate counter clockwise */
    __u32 colorspace;		/* colorspace for FOURCC-based modes */
    __u32 reserved[4];		/* Reserved for future compatibility */
};

struct fb_fix_screeninfo {
    char id[16];			/* identification string eg "TT Builtin" */
    unsigned long smem_start;	/* Start of frame buffer mem */
                    /* (physical address) */
    __u32 smem_len;			/* Length of frame buffer mem */
    __u32 type;			/* see FB_TYPE_*		*/
    __u32 type_aux;			/* Interleave for interleaved Planes */
    __u32 visual;			/* see FB_VISUAL_*		*/
    __u16 xpanstep;			/* zero if no hardware panning  */
    __u16 ypanstep;			/* zero if no hardware panning  */
    __u16 ywrapstep;		/* zero if no hardware ywrap    */
    __u32 line_length;		/* length of a line in bytes    */
    unsigned long mmio_start;	/* Start of Memory Mapped I/O   */
                    /* (physical address) */
    __u32 mmio_len;			/* Length of Memory Mapped I/O  */
    __u32 accel;			/* Indicate to driver which	*/
                    /*  specific chip/card we have	*/
    __u16 capabilities;		/* see FB_CAP_*			*/
    __u16 reserved[2];		/* Reserved for future compatibility */
};

struct mxcfb_color_key {
    // TODO: ?
};

struct mxcfb_gbl_alpha {
    // TODO: ?
};

struct mxcfb_loc_alpha {
    // TODO: ?
};

typedef struct fb_dev_info{
    int fb_node;
    char fb_dev[100];
    int fb_fix_bbp;
    struct fb_fix_screeninfo fb_fix;
    struct fb_var_screeninfo fb_var;
    struct mxcfb_color_key color_key;
    struct mxcfb_gbl_alpha g_alpha;
    struct mxcfb_loc_alpha l_alpha;
    char *fb_buf;
    char *fbuf_bak;
    int fb_bf_len;
}fb_dev_st;

//
enum BarcodeDecoderError
{
};

class BarcodeDecoder
{
public:
    BarcodeDecoderError ScanImage(IplImage &_image, std::string &_code_str, std::string &_type_name)
    {
        Q_UNUSED(_image)
        Q_UNUSED(_code_str)
        Q_UNUSED(_type_name)
        return (BarcodeDecoderError)0;
    }
};

#endif // TESTDESKTOP

