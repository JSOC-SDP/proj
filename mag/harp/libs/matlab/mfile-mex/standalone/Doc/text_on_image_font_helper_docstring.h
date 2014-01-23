
/*
 * This file is generated by a script, do not edit.
 *
 * This documentation string was generated from
 * a C comment block within a mex program
 * by `doc2docstring.py' on Thu Jun 23 18:26:49 2011.
 */
 
static const char docstring[] =
	"text_on_image_font_helper: render characters of a font for use later\n"
	"\n"
	" [bm,cpos] = text_on_image_font_helper(fontfile, dict, height, width)\n"
	" * Renders a set of characters, dict, into a 3d stack of bitmaps bm,\n"
	" using a given TrueType font file, and a given height and width.\n"
	" The per-character positions (start and end) are given in cpos.\n"
	" * The font file name is given in `fontfile'.  Reasonable\n"
	" truetype (.ttf) files work, and some truetype collections (.ttc/.ttfc)\n"
	" work.  Some truetype collections (e.g., Apple's Menlo) do not work.\n"
	" However, ordinary .ttf files can be extracted from .ttc's.\n"
	" * The list of characters to convert is given in `dict'.\n"
	" * The dimensions of each output character are the same (which might\n"
	" be undesirable sometimes).  This can be addressed using the wid output.\n"
	" * The height argument is in this format:\n"
	"    [char_height allow_space_for_descenders top_pad bottom_pad]\n"
	" char_height is in pixels.  By default, the highest ascender and lowest\n"
	" descender in the font fit into char_height.  If allow_space_for_descenders\n"
	" is false, room is made for ascenders only, which is useful for, e.g.,\n"
	" maximizing compactness in digit-only text.  The last two entries give\n"
	" top and bottom padding, respectively.  They can be negative, which\n"
	" is helpful if, for example, too much space is being left for\n"
	" ascenders.\n"
	" * The width of each bitmap is enough to accommodate the character\n"
	" width(1) (which is an integer value supplied as a double, but\n"
	" representing a unicode character).  Additionally, extra padding\n"
	" to the left and right can be given in width(2) and width(3), which\n"
	" can again be negative to soak up extra space.\n"
	" * The default height is 32 pixels, accommodating both ascenders\n"
	" and descenders, and no extra padding.  The height entries can be\n"
	" cut off at any point, so [], [32], [32 1], [32 1 -1], and [32 1 -1 -2]\n"
	" are all legal inputs.\n"
	" * The default character-width is that of `0', and no extra padding.\n"
	" As with height, the entries may be cut off at any point.\n"
	" * The bitmap is in raster scan order (left -> right, top -> bottom).\n"
	" To display in matlab, use image(bm(:,:,nchar)').\n"
	" * The character positions can be used to extract the original character\n"
	" bitmap within the TrueType font from its embedding into a standard-size\n"
	" bitmap.  It is offset by 1 and clipped to the valid range, so you can\n"
	" use: bm(cpos(1,nchar):cpos(2,nchar),:,nchar).\n"
	"\n"
	" Inputs:\n"
	"  string font\n"
	"  string dict(n)\n"
	"  opt int height(0, 1, 2, 3, or 4) = [32 1 0 0]\n"
	"  opt int width(0, 1, 2, or 3) = [double('0') 0 0]\n"
	"\n"
	" Outputs:\n"
	"  uint8 bm(wd,ht,n)\n"
	"  opt real cpos(2,n)\n"
	"\n"
	" See Also:  text_on_image\n"
	"\n"
	"";

/* End of generated file */
