/*
Modified version of stb_truetype: https://github.com/nothings/stb/blob/master/stb_truetype.h
Changes:
- replace rasterizer with CPU-side Slug algorithm by Eric Lengyel https://terathon.com/blog/decade-slug.html
- remove unneeded functionality
- remove "stbtt" namespace prefix
- rename functions, mostly from camel case to snake case
*/
#include <bane.h>
#include <math.h>

#define ifloor(x)   ((int) floor(x))
#define iceil(x)    ((int) ceil(x))

//////////////////////////////////////////////////////////////////////////
//
// buf helpers to parse data from file
//

typedef struct
{
   unsigned char *data;
   int cursor;
   int size;
} buf;

static uint8_t buf_get8(buf *b)
{
   if (b->cursor >= b->size)
      return 0;
   return b->data[b->cursor++];
}

static uint8_t buf_peek8(buf *b)
{
   if (b->cursor >= b->size)
      return 0;
   return b->data[b->cursor];
}

static void buf_seek(buf *b, int o)
{
   assert(!(o > b->size || o < 0));
   b->cursor = (o > b->size || o < 0) ? b->size : o;
}

static void buf_skip(buf *b, int o)
{
   buf_seek(b, b->cursor + o);
}

static uint32_t buf_get(buf *b, int n)
{
   uint32_t v = 0;
   int i;
   assert(n >= 1 && n <= 4);
   for (i = 0; i < n; i++)
      v = (v << 8) | buf_get8(b);
   return v;
}

static buf _new_buf(const void *p, size_t size)
{
   buf r;
   assert(size < 0x40000000);
   r.data = (uint8_t*) p;
   r.size = (int) size;
   r.cursor = 0;
   return r;
}

#define buf_get16(b)  buf_get((b), 2)
#define buf_get32(b)  buf_get((b), 4)

static buf buf_range(const buf *b, int o, int s)
{
   buf r = _new_buf(NULL, 0);
   if (o < 0 || s < 0 || o > b->size || s > b->size - o) return r;
   r.data = b->data + o;
   r.size = s;
   return r;
}

static buf _cff_get_index(buf *b)
{
   int count, start, offsize;
   start = b->cursor;
   count = buf_get16(b);
   if (count) {
      offsize = buf_get8(b);
      assert(offsize >= 1 && offsize <= 4);
      buf_skip(b, offsize * count);
      buf_skip(b, buf_get(b, offsize) - 1);
   }
   return buf_range(b, start, b->cursor - start);
}

static uint32_t _cff_int(buf *b)
{
   int b0 = buf_get8(b);
   if (b0 >= 32 && b0 <= 246)       return b0 - 139;
   else if (b0 >= 247 && b0 <= 250) return (b0 - 247)*256 + buf_get8(b) + 108;
   else if (b0 >= 251 && b0 <= 254) return -(b0 - 251)*256 - buf_get8(b) - 108;
   else if (b0 == 28)               return buf_get16(b);
   else if (b0 == 29)               return buf_get32(b);
   assert(0);
   return 0;
}

static void _cff_skip_operand(buf *b) {
   int v, b0 = buf_peek8(b);
   assert(b0 >= 28);
   if (b0 == 30) {
      buf_skip(b, 1);
      while (b->cursor < b->size) {
         v = buf_get8(b);
         if ((v & 0xF) == 0xF || (v >> 4) == 0xF)
            break;
      }
   } else {
      _cff_int(b);
   }
}

static buf _dict_get(buf *b, int key)
{
   buf_seek(b, 0);
   while (b->cursor < b->size) {
      int start = b->cursor, end, op;
      while (buf_peek8(b) >= 28)
         _cff_skip_operand(b);
      end = b->cursor;
      op = buf_get8(b);
      if (op == 12)  op = buf_get8(b) | 0x100;
      if (op == key) return buf_range(b, start, end-start);
   }
   return buf_range(b, 0, 0);
}

static void _dict_get_ints(buf *b, int key, int outcount, uint32_t *out)
{
   int i;
   buf operands = _dict_get(b, key);
   for (i = 0; i < outcount && operands.cursor < operands.size; i++)
      out[i] = _cff_int(&operands);
}

static int _cff_index_count(buf *b)
{
   buf_seek(b, 0);
   return buf_get16(b);
}

static buf _cff_index_get(buf b, int i)
{
   int count, offsize, start, end;
   buf_seek(&b, 0);
   count = buf_get16(&b);
   offsize = buf_get8(&b);
   assert(i >= 0 && i < count);
   assert(offsize >= 1 && offsize <= 4);
   buf_skip(&b, i*offsize);
   start = buf_get(&b, offsize);
   end = buf_get(&b, offsize);
   return buf_range(&b, 2+(count+1)*offsize+start, end - start);
}

//////////////////////////////////////////////////////////////////////////
//
// accessors to parse data from file
//

// on platforms that don't allow misaligned reads, if we want to allow
// truetype fonts that aren't padded to alignment, define ALLOW_UNALIGNED_TRUETYPE

#define ttBYTE(p)     (* (uint8_t *) (p))
#define ttCHAR(p)     (* (int8_t *) (p))
#define ttFixed(p)    ttLONG(p)

static uint16_t ttUSHORT(uint8_t *p) { return p[0]*256 + p[1]; }
static int16_t ttSHORT(uint8_t *p)   { return p[0]*256 + p[1]; }
static uint32_t ttULONG(uint8_t *p)  { return (p[0]<<24) + (p[1]<<16) + (p[2]<<8) + p[3]; }
static int32_t ttLONG(uint8_t *p)    { return (p[0]<<24) + (p[1]<<16) + (p[2]<<8) + p[3]; }

#define tag4(p,c0,c1,c2,c3) ((p)[0] == (c0) && (p)[1] == (c1) && (p)[2] == (c2) && (p)[3] == (c3))
#define tag(p,str)           tag4(p,str[0],str[1],str[2],str[3])

static int _isfont(uint8_t *font)
{
   // check the version number
   if (tag4(font, '1',0,0,0))  return 1; // TrueType 1
   if (tag(font, "typ1"))   return 1; // TrueType with type 1 font -- we don't support this!
   if (tag(font, "OTTO"))   return 1; // OpenType with CFF
   if (tag4(font, 0,1,0,0)) return 1; // OpenType 1.0
   if (tag(font, "true"))   return 1; // Apple specification for TrueType fonts
   return 0;
}

// @OPTIMIZE: binary search
static uint32_t _find_table(uint8_t *data, uint32_t fontstart, const char *tag)
{
   int32_t num_tables = ttUSHORT(data+fontstart+4);
   uint32_t tabledir = fontstart + 12;
   int32_t i;
   for (i=0; i < num_tables; ++i) {
      uint32_t loc = tabledir + 16*i;
      if (tag(data+loc+0, tag))
         return ttULONG(data+loc+8);
   }
   return 0;
}

static int font_offset_for_index_internal(unsigned char *font_collection, int index)
{
   // if it's just a font, there's only one valid index
   if (_isfont(font_collection))
      return index == 0 ? 0 : -1;

   // check if it's a TTC
   if (tag(font_collection, "ttcf")) {
      // version 1?
      if (ttULONG(font_collection+4) == 0x00010000 || ttULONG(font_collection+4) == 0x00020000) {
         int32_t n = ttLONG(font_collection+8);
         if (index >= n)
            return -1;
         return ttULONG(font_collection+12+index*4);
      }
   }
   return -1;
}

static int font_count_internal(unsigned char *font_collection)
{
   // if it's just a font, there's only one valid font
   if (_isfont(font_collection))
      return 1;

   // check if it's a TTC
   if (tag(font_collection, "ttcf")) {
      // version 1?
      if (ttULONG(font_collection+4) == 0x00010000 || ttULONG(font_collection+4) == 0x00020000) {
         return ttLONG(font_collection+8);
      }
   }
   return 0;
}

static buf _get_subrs(buf cff, buf fontdict)
{
   uint32_t subrsoff = 0, private_loc[2] = { 0, 0 };
   buf pdict;
   _dict_get_ints(&fontdict, 18, 2, private_loc);
   if (!private_loc[1] || !private_loc[0]) return _new_buf(NULL, 0);
   pdict = buf_range(&cff, private_loc[1], private_loc[0]);
   _dict_get_ints(&pdict, 19, 1, &subrsoff);
   if (!subrsoff) return _new_buf(NULL, 0);
   buf_seek(&cff, private_loc[1]+subrsoff);
   return _cff_get_index(&cff);
}

typedef struct fontinfo {
   unsigned char  * data;              // pointer to .ttf file
   int              fontstart;         // offset of start of font

   int numGlyphs;                     // number of glyphs, needed for range checking

   int loca,head,glyf,hhea,hmtx,kern,gpos,svg; // table locations as offset from start of .ttf
   int index_map;                     // a cmap mapping for our chosen character encoding
   int indexToLocFormat;              // format needed to map from glyph index to glyph

   buf cff;                    // cff font data
   buf charstrings;            // the charstring index
   buf gsubrs;                 // global charstring subroutines index
   buf subrs;                  // private charstring subroutines index
   buf fontdicts;              // array of font dicts
   buf fdselect;               // map from glyph to fontdict
} fontinfo;

void font_destroy(fontinfo *info) {
   free(info);
}

static int font_init_internal(fontinfo *info, unsigned char *data, int fontstart)
{
   uint32_t cmap, t;
   int32_t i,numTables;

   info->data = data;
   info->fontstart = fontstart;
   info->cff = _new_buf(NULL, 0);

   cmap = _find_table(data, fontstart, "cmap");       // required
   info->loca = _find_table(data, fontstart, "loca"); // required
   info->head = _find_table(data, fontstart, "head"); // required
   info->glyf = _find_table(data, fontstart, "glyf"); // required
   info->hhea = _find_table(data, fontstart, "hhea"); // required
   info->hmtx = _find_table(data, fontstart, "hmtx"); // required
   info->kern = _find_table(data, fontstart, "kern"); // not required
   info->gpos = _find_table(data, fontstart, "GPOS"); // not required

   if (!cmap || !info->head || !info->hhea || !info->hmtx)
      return 0;
   if (info->glyf) {
      // required for truetype
      if (!info->loca) return 0;
   } else {
      // initialization for CFF / Type2 fonts (OTF)
      buf b, topdict, topdictidx;
      uint32_t cstype = 2, charstrings = 0, fdarrayoff = 0, fdselectoff = 0;
      uint32_t cff;

      cff = _find_table(data, fontstart, "CFF ");
      if (!cff) return 0;

      info->fontdicts = _new_buf(NULL, 0);
      info->fdselect = _new_buf(NULL, 0);

      // @TODO this should use size from table (not 512MB)
      info->cff = _new_buf(data+cff, 512*1024*1024);
      b = info->cff;

      // read the header
      buf_skip(&b, 2);
      buf_seek(&b, buf_get8(&b)); // hdrsize

      // @TODO the name INDEX could list multiple fonts,
      // but we just use the first one.
      _cff_get_index(&b);  // name INDEX
      topdictidx = _cff_get_index(&b);
      topdict = _cff_index_get(topdictidx, 0);
      _cff_get_index(&b);  // string INDEX
      info->gsubrs = _cff_get_index(&b);

      _dict_get_ints(&topdict, 17, 1, &charstrings);
      _dict_get_ints(&topdict, 0x100 | 6, 1, &cstype);
      _dict_get_ints(&topdict, 0x100 | 36, 1, &fdarrayoff);
      _dict_get_ints(&topdict, 0x100 | 37, 1, &fdselectoff);
      info->subrs = _get_subrs(b, topdict);

      // we only support Type 2 charstrings
      if (cstype != 2) return 0;
      if (charstrings == 0) return 0;

      if (fdarrayoff) {
         // looks like a CID font
         if (!fdselectoff) return 0;
         buf_seek(&b, fdarrayoff);
         info->fontdicts = _cff_get_index(&b);
         info->fdselect = buf_range(&b, fdselectoff, b.size-fdselectoff);
      }

      buf_seek(&b, charstrings);
      info->charstrings = _cff_get_index(&b);
   }

   t = _find_table(data, fontstart, "maxp");
   if (t)
      info->numGlyphs = ttUSHORT(data+t+4);
   else
      info->numGlyphs = 0xffff;

   info->svg = -1;

   // find a cmap encoding table we understand *now* to avoid searching
   // later. (todo: could make this installable)
   // the same regardless of glyph.
   numTables = ttUSHORT(data + cmap + 2);
   info->index_map = 0;
   for (i=0; i < numTables; ++i) {
      uint32_t encoding_record = cmap + 4 + 8 * i;
      // find an encoding we understand:
      switch(ttUSHORT(data+encoding_record)) {
         case PLATFORM_ID_MICROSOFT:
            switch (ttUSHORT(data+encoding_record+2)) {
               case MS_EID_UNICODE_BMP:
               case MS_EID_UNICODE_FULL:
                  // MS/Unicode
                  info->index_map = cmap + ttULONG(data+encoding_record+4);
                  break;
            }
            break;
        case PLATFORM_ID_UNICODE:
            // Mac/iOS has these
            // all the encodingIDs are unicode, so we don't bother to check it
            info->index_map = cmap + ttULONG(data+encoding_record+4);
            break;
      }
   }
   if (info->index_map == 0)
      return 0;

   info->indexToLocFormat = ttUSHORT(data+info->head + 50);
   return 1;
}

int font_glyph_index(const fontinfo *info, int unicode_codepoint)
{
   uint8_t *data = info->data;
   uint32_t index_map = info->index_map;

   uint16_t format = ttUSHORT(data + index_map + 0);
   if (format == 0) { // apple byte encoding
      int32_t bytes = ttUSHORT(data + index_map + 2);
      if (unicode_codepoint < bytes-6)
         return ttBYTE(data + index_map + 6 + unicode_codepoint);
      return 0;
   } else if (format == 6) {
      uint32_t first = ttUSHORT(data + index_map + 6);
      uint32_t count = ttUSHORT(data + index_map + 8);
      if ((uint32_t) unicode_codepoint >= first && (uint32_t) unicode_codepoint < first+count)
         return ttUSHORT(data + index_map + 10 + (unicode_codepoint - first)*2);
      return 0;
   } else if (format == 2) {
      assert(0); // @TODO: high-byte mapping for japanese/chinese/korean
      return 0;
   } else if (format == 4) { // standard mapping for windows fonts: binary search collection of ranges
      uint16_t segcount = ttUSHORT(data+index_map+6) >> 1;
      uint16_t searchRange = ttUSHORT(data+index_map+8) >> 1;
      uint16_t entrySelector = ttUSHORT(data+index_map+10);
      uint16_t rangeShift = ttUSHORT(data+index_map+12) >> 1;

      // do a binary search of the segments
      uint32_t endCount = index_map + 14;
      uint32_t search = endCount;

      if (unicode_codepoint > 0xffff)
         return 0;

      // they lie from endCount .. endCount + segCount
      // but searchRange is the nearest power of two, so...
      if (unicode_codepoint >= ttUSHORT(data + search + rangeShift*2))
         search += rangeShift*2;

      // now decrement to bias correctly to find smallest
      search -= 2;
      while (entrySelector) {
         uint16_t end;
         searchRange >>= 1;
         end = ttUSHORT(data + search + searchRange*2);
         if (unicode_codepoint > end)
            search += searchRange*2;
         --entrySelector;
      }
      search += 2;

      {
         uint16_t offset, start, last;
         uint16_t item = (uint16_t) ((search - endCount) >> 1);

         start = ttUSHORT(data + index_map + 14 + segcount*2 + 2 + 2*item);
         last = ttUSHORT(data + endCount + 2*item);
         if (unicode_codepoint < start || unicode_codepoint > last)
            return 0;

         offset = ttUSHORT(data + index_map + 14 + segcount*6 + 2 + 2*item);
         if (offset == 0)
            return (uint16_t) (unicode_codepoint + ttSHORT(data + index_map + 14 + segcount*4 + 2 + 2*item));

         return ttUSHORT(data + offset + (unicode_codepoint-start)*2 + index_map + 14 + segcount*6 + 2 + 2*item);
      }
   } else if (format == 12 || format == 13) {
      uint32_t ngroups = ttULONG(data+index_map+12);
      int32_t low,high;
      low = 0; high = (int32_t)ngroups;
      // Binary search the right group.
      while (low < high) {
         int32_t mid = low + ((high-low) >> 1); // rounds down, so low <= mid < high
         uint32_t start_char = ttULONG(data+index_map+16+mid*12);
         uint32_t end_char = ttULONG(data+index_map+16+mid*12+4);
         if ((uint32_t) unicode_codepoint < start_char)
            high = mid;
         else if ((uint32_t) unicode_codepoint > end_char)
            low = mid+1;
         else {
            uint32_t start_glyph = ttULONG(data+index_map+16+mid*12+8);
            if (format == 12)
               return start_glyph + unicode_codepoint-start_char;
            else // format == 13
               return start_glyph;
         }
      }
      return 0; // not found
   }
   // @TODO
   assert(0);
   return 0;
}

typedef struct
{
    int16_t x,y,cx,cy,cx1,cy1;
    unsigned char type,padding;
} vertex;

static void vertex_set(vertex *v, uint8_t type, int32_t x, int32_t y, int32_t cx, int32_t cy)
{
   v->type = type;
   v->x = (int16_t) x;
   v->y = (int16_t) y;
   v->cx = (int16_t) cx;
   v->cy = (int16_t) cy;
}

static int _glyph_offset(const fontinfo *info, int glyph_index)
{
   int g1,g2;

   assert(!info->cff.size);

   if (glyph_index >= info->numGlyphs) return -1; // glyph index out of range
   if (info->indexToLocFormat >= 2)    return -1; // unknown index->glyph map format

   if (info->indexToLocFormat == 0) {
      g1 = info->glyf + ttUSHORT(info->data + info->loca + glyph_index * 2) * 2;
      g2 = info->glyf + ttUSHORT(info->data + info->loca + glyph_index * 2 + 2) * 2;
   } else {
      g1 = info->glyf + ttULONG (info->data + info->loca + glyph_index * 4);
      g2 = info->glyf + ttULONG (info->data + info->loca + glyph_index * 4 + 4);
   }

   return g1==g2 ? -1 : g1; // if length is 0, return -1
}

static int _glyph_info_T2(const fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1);

int GetGlyphBox(const fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1)
{
   if (info->cff.size) {
      _glyph_info_T2(info, glyph_index, x0, y0, x1, y1);
   } else {
      int g = _glyph_offset(info, glyph_index);
      if (g < 0) return 0;

      if (x0) *x0 = ttSHORT(info->data + g + 2);
      if (y0) *y0 = ttSHORT(info->data + g + 4);
      if (x1) *x1 = ttSHORT(info->data + g + 6);
      if (y1) *y1 = ttSHORT(info->data + g + 8);
   }
   return 1;
}

enum {vmove=1, vline, vcurve, vcubic};

static int _close_shape(vertex *vertices, int num_vertices, int was_off, int start_off,
    int32_t sx, int32_t sy, int32_t scx, int32_t scy, int32_t cx, int32_t cy)
{
   if (start_off) {
      if (was_off)
         vertex_set(&vertices[num_vertices++], vcurve, (cx+scx)>>1, (cy+scy)>>1, cx,cy);
      vertex_set(&vertices[num_vertices++], vcurve, sx,sy,scx,scy);
   } else {
      if (was_off)
         vertex_set(&vertices[num_vertices++], vcurve,sx,sy,cx,cy);
      else
         vertex_set(&vertices[num_vertices++], vline,sx,sy,0,0);
   }
   return num_vertices;
}

static int glyph_shape(const fontinfo *info, int glyph_index, vertex **pvertices);

static int glyph_shape_TT(const fontinfo *info, int glyph_index, vertex **pvertices)
{
   int16_t numberOfContours;
   uint8_t *endPtsOfContours;
   uint8_t *data = info->data;
   vertex *vertices=0;
   int num_vertices=0;
   int g = _glyph_offset(info, glyph_index);

   *pvertices = NULL;

   if (g < 0) return 0;

   numberOfContours = ttSHORT(data + g);

   if (numberOfContours > 0) {
      uint8_t flags=0,flagcount;
      int32_t ins, i,j=0,m,n, next_move, was_off=0, off, start_off=0;
      int32_t x,y,cx,cy,sx,sy, scx,scy;
      uint8_t *points;
      endPtsOfContours = (data + g + 10);
      ins = ttUSHORT(data + g + 10 + numberOfContours * 2);
      points = data + g + 10 + numberOfContours * 2 + 2 + ins;

      n = 1+ttUSHORT(endPtsOfContours + numberOfContours*2-2);

      m = n + 2*numberOfContours;  // a loose bound on how many vertices we might need
      vertices = (vertex *) malloc(m * sizeof(vertices[0]));
      if (vertices == 0)
         return 0;

      next_move = 0;
      flagcount=0;

      // in first pass, we load uninterpreted data into the allocated array
      // above, shifted to the end of the array so we won't overwrite it when
      // we create our final data starting from the front

      off = m - n; // starting offset for uninterpreted data, regardless of how m ends up being calculated

      // first load flags

      for (i=0; i < n; ++i) {
         if (flagcount == 0) {
            flags = *points++;
            if (flags & 8)
               flagcount = *points++;
         } else
            --flagcount;
         vertices[off+i].type = flags;
      }

      // now load x coordinates
      x=0;
      for (i=0; i < n; ++i) {
         flags = vertices[off+i].type;
         if (flags & 2) {
            int16_t dx = *points++;
            x += (flags & 16) ? dx : -dx; // ???
         } else {
            if (!(flags & 16)) {
               x = x + (int16_t) (points[0]*256 + points[1]);
               points += 2;
            }
         }
         vertices[off+i].x = (int16_t) x;
      }

      // now load y coordinates
      y=0;
      for (i=0; i < n; ++i) {
         flags = vertices[off+i].type;
         if (flags & 4) {
            int16_t dy = *points++;
            y += (flags & 32) ? dy : -dy; // ???
         } else {
            if (!(flags & 32)) {
               y = y + (int16_t) (points[0]*256 + points[1]);
               points += 2;
            }
         }
         vertices[off+i].y = (int16_t) y;
      }

      // now convert them to our format
      num_vertices=0;
      sx = sy = cx = cy = scx = scy = 0;
      for (i=0; i < n; ++i) {
         flags = vertices[off+i].type;
         x     = (int16_t) vertices[off+i].x;
         y     = (int16_t) vertices[off+i].y;

         if (next_move == i) {
            if (i != 0)
               num_vertices = _close_shape(vertices, num_vertices, was_off, start_off, sx,sy,scx,scy,cx,cy);

            // now start the new one
            start_off = !(flags & 1);
            if (start_off) {
               // if we start off with an off-curve point, then when we need to find a point on the curve
               // where we can start, and we need to save some state for when we wraparound.
               scx = x;
               scy = y;
               if (!(vertices[off+i+1].type & 1)) {
                  // next point is also a curve point, so interpolate an on-point curve
                  sx = (x + (int32_t) vertices[off+i+1].x) >> 1;
                  sy = (y + (int32_t) vertices[off+i+1].y) >> 1;
               } else {
                  // otherwise just use the next point as our start point
                  sx = (int32_t) vertices[off+i+1].x;
                  sy = (int32_t) vertices[off+i+1].y;
                  ++i; // we're using point i+1 as the starting point, so skip it
               }
            } else {
               sx = x;
               sy = y;
            }
            vertex_set(&vertices[num_vertices++], vmove,sx,sy,0,0);
            was_off = 0;
            next_move = 1 + ttUSHORT(endPtsOfContours+j*2);
            ++j;
         } else {
            if (!(flags & 1)) { // if it's a curve
               if (was_off) // two off-curve control points in a row means interpolate an on-curve midpoint
                  vertex_set(&vertices[num_vertices++], vcurve, (cx+x)>>1, (cy+y)>>1, cx, cy);
               cx = x;
               cy = y;
               was_off = 1;
            } else {
               if (was_off)
                  vertex_set(&vertices[num_vertices++], vcurve, x,y, cx, cy);
               else
                  vertex_set(&vertices[num_vertices++], vline, x,y,0,0);
               was_off = 0;
            }
         }
      }
      num_vertices = _close_shape(vertices, num_vertices, was_off, start_off, sx,sy,scx,scy,cx,cy);
   } else if (numberOfContours < 0) {
      // Compound shapes.
      int more = 1;
      uint8_t *comp = data + g + 10;
      num_vertices = 0;
      vertices = 0;
      while (more) {
         uint16_t flags, gidx;
         int comp_num_verts = 0, i;
         vertex *comp_verts = 0, *tmp = 0;
         float mtx[6] = {1,0,0,1,0,0}, m, n;

         flags = ttSHORT(comp); comp+=2;
         gidx = ttSHORT(comp); comp+=2;

         if (flags & 2) { // XY values
            if (flags & 1) { // shorts
               mtx[4] = ttSHORT(comp); comp+=2;
               mtx[5] = ttSHORT(comp); comp+=2;
            } else {
               mtx[4] = ttCHAR(comp); comp+=1;
               mtx[5] = ttCHAR(comp); comp+=1;
            }
         }
         else {
            // @TODO handle matching point
            assert(0);
         }
         if (flags & (1<<3)) { // WE_HAVE_A_SCALE
            mtx[0] = mtx[3] = ttSHORT(comp)/16384.0f; comp+=2;
            mtx[1] = mtx[2] = 0;
         } else if (flags & (1<<6)) { // WE_HAVE_AN_X_AND_YSCALE
            mtx[0] = ttSHORT(comp)/16384.0f; comp+=2;
            mtx[1] = mtx[2] = 0;
            mtx[3] = ttSHORT(comp)/16384.0f; comp+=2;
         } else if (flags & (1<<7)) { // WE_HAVE_A_TWO_BY_TWO
            mtx[0] = ttSHORT(comp)/16384.0f; comp+=2;
            mtx[1] = ttSHORT(comp)/16384.0f; comp+=2;
            mtx[2] = ttSHORT(comp)/16384.0f; comp+=2;
            mtx[3] = ttSHORT(comp)/16384.0f; comp+=2;
         }

         // Find transformation scales.
         m = (float) sqrt(mtx[0]*mtx[0] + mtx[1]*mtx[1]);
         n = (float) sqrt(mtx[2]*mtx[2] + mtx[3]*mtx[3]);

         // Get indexed glyph.
         comp_num_verts = glyph_shape(info, gidx, &comp_verts);
         if (comp_num_verts > 0) {
            // Transform vertices.
            for (i = 0; i < comp_num_verts; ++i) {
               vertex* v = &comp_verts[i];
               int16_t x,y;
               x=v->x; y=v->y;
               v->x = (int16_t)(m * (mtx[0]*x + mtx[2]*y + mtx[4]));
               v->y = (int16_t)(n * (mtx[1]*x + mtx[3]*y + mtx[5]));
               x=v->cx; y=v->cy;
               v->cx = (int16_t)(m * (mtx[0]*x + mtx[2]*y + mtx[4]));
               v->cy = (int16_t)(n * (mtx[1]*x + mtx[3]*y + mtx[5]));
            }
            // Append vertices.
            tmp = (vertex*)malloc((num_vertices+comp_num_verts)*sizeof(vertex));
            if (!tmp) {
               if (vertices) free(vertices);
               if (comp_verts) free(comp_verts);
               return 0;
            }
            if (num_vertices > 0 && vertices) memcpy(tmp, vertices, num_vertices*sizeof(vertex));
            memcpy(tmp+num_vertices, comp_verts, comp_num_verts*sizeof(vertex));
            if (vertices) free(vertices);
            vertices = tmp;
            free(comp_verts);
            num_vertices += comp_num_verts;
         }
         // More components ?
         more = flags & (1<<5);
      }
   } else {
      // numberOfCounters == 0, do nothing
   }

   *pvertices = vertices;
   return num_vertices;
}

typedef struct
{
   int bounds;
   int started;
   float first_x, first_y;
   float x, y;
   int32_t min_x, max_x, min_y, max_y;

   vertex *pvertices;
   int num_vertices;
} _csctx;

#define _CSCTX_INIT(bounds) {bounds,0, 0,0, 0,0, 0,0,0,0, NULL, 0}

static void _track_vertex(_csctx *c, int32_t x, int32_t y)
{
   if (x > c->max_x || !c->started) c->max_x = x;
   if (y > c->max_y || !c->started) c->max_y = y;
   if (x < c->min_x || !c->started) c->min_x = x;
   if (y < c->min_y || !c->started) c->min_y = y;
   c->started = 1;
}

static void _csctx_v(_csctx *c, uint8_t type, int32_t x, int32_t y, int32_t cx, int32_t cy, int32_t cx1, int32_t cy1)
{
   if (c->bounds) {
      _track_vertex(c, x, y);
      if (type == vcubic) {
         _track_vertex(c, cx, cy);
         _track_vertex(c, cx1, cy1);
      }
   } else {
      vertex_set(&c->pvertices[c->num_vertices], type, x, y, cx, cy);
      c->pvertices[c->num_vertices].cx1 = (int16_t) cx1;
      c->pvertices[c->num_vertices].cy1 = (int16_t) cy1;
   }
   c->num_vertices++;
}

static void _csctx_close_shape(_csctx *ctx)
{
   if (ctx->first_x != ctx->x || ctx->first_y != ctx->y)
      _csctx_v(ctx, vline, (int)ctx->first_x, (int)ctx->first_y, 0, 0, 0, 0);
}

static void _csctx_rmove_to(_csctx *ctx, float dx, float dy)
{
   _csctx_close_shape(ctx);
   ctx->first_x = ctx->x = ctx->x + dx;
   ctx->first_y = ctx->y = ctx->y + dy;
   _csctx_v(ctx, vmove, (int)ctx->x, (int)ctx->y, 0, 0, 0, 0);
}

static void _csctx_rline_to(_csctx *ctx, float dx, float dy)
{
   ctx->x += dx;
   ctx->y += dy;
   _csctx_v(ctx, vline, (int)ctx->x, (int)ctx->y, 0, 0, 0, 0);
}

static void _csctx_rccurve_to(_csctx *ctx, float dx1, float dy1, float dx2, float dy2, float dx3, float dy3)
{
   float cx1 = ctx->x + dx1;
   float cy1 = ctx->y + dy1;
   float cx2 = cx1 + dx2;
   float cy2 = cy1 + dy2;
   ctx->x = cx2 + dx3;
   ctx->y = cy2 + dy3;
   _csctx_v(ctx, vcubic, (int)ctx->x, (int)ctx->y, (int)cx1, (int)cy1, (int)cx2, (int)cy2);
}

static buf _get_subr(buf idx, int n)
{
   int count = _cff_index_count(&idx);
   int bias = 107;
   if (count >= 33900)
      bias = 32768;
   else if (count >= 1240)
      bias = 1131;
   n += bias;
   if (n < 0 || n >= count)
      return _new_buf(NULL, 0);
   return _cff_index_get(idx, n);
}

static buf _cid_get_glyph_subrs(const fontinfo *info, int glyph_index)
{
   buf fdselect = info->fdselect;
   int nranges, start, end, v, fmt, fdselector = -1, i;

   buf_seek(&fdselect, 0);
   fmt = buf_get8(&fdselect);
   if (fmt == 0) {
      // untested
      buf_skip(&fdselect, glyph_index);
      fdselector = buf_get8(&fdselect);
   } else if (fmt == 3) {
      nranges = buf_get16(&fdselect);
      start = buf_get16(&fdselect);
      for (i = 0; i < nranges; i++) {
         v = buf_get8(&fdselect);
         end = buf_get16(&fdselect);
         if (glyph_index >= start && glyph_index < end) {
            fdselector = v;
            break;
         }
         start = end;
      }
   }
   if (fdselector == -1) _new_buf(NULL, 0);
   return _get_subrs(info->cff, _cff_index_get(info->fontdicts, fdselector));
}

static int _run_charstring(const fontinfo *info, int glyph_index, _csctx *c)
{
   int in_header = 1, maskbits = 0, subr_stack_height = 0, sp = 0, v, i, b0;
   int has_subrs = 0, clear_stack;
   float s[48];
   buf subr_stack[10], subrs = info->subrs, b;
   float f;

#define _CSERR(s) (0)

   // this currently ignores the initial width value, which isn't needed if we have hmtx
   b = _cff_index_get(info->charstrings, glyph_index);
   while (b.cursor < b.size) {
      i = 0;
      clear_stack = 1;
      b0 = buf_get8(&b);
      switch (b0) {
      // @TODO implement hinting
      case 0x13: // hintmask
      case 0x14: // cntrmask
         if (in_header)
            maskbits += (sp / 2); // implicit "vstem"
         in_header = 0;
         buf_skip(&b, (maskbits + 7) / 8);
         break;

      case 0x01: // hstem
      case 0x03: // vstem
      case 0x12: // hstemhm
      case 0x17: // vstemhm
         maskbits += (sp / 2);
         break;

      case 0x15: // rmoveto
         in_header = 0;
         if (sp < 2) return _CSERR("rmoveto stack");
         _csctx_rmove_to(c, s[sp-2], s[sp-1]);
         break;
      case 0x04: // vmoveto
         in_header = 0;
         if (sp < 1) return _CSERR("vmoveto stack");
         _csctx_rmove_to(c, 0, s[sp-1]);
         break;
      case 0x16: // hmoveto
         in_header = 0;
         if (sp < 1) return _CSERR("hmoveto stack");
         _csctx_rmove_to(c, s[sp-1], 0);
         break;

      case 0x05: // rlineto
         if (sp < 2) return _CSERR("rlineto stack");
         for (; i + 1 < sp; i += 2)
            _csctx_rline_to(c, s[i], s[i+1]);
         break;

      // hlineto/vlineto and vhcurveto/hvcurveto alternate horizontal and vertical
      // starting from a different place.

      case 0x07: // vlineto
         if (sp < 1) return _CSERR("vlineto stack");
         goto vlineto;
      case 0x06: // hlineto
         if (sp < 1) return _CSERR("hlineto stack");
         for (;;) {
            if (i >= sp) break;
            _csctx_rline_to(c, s[i], 0);
            i++;
      vlineto:
            if (i >= sp) break;
            _csctx_rline_to(c, 0, s[i]);
            i++;
         }
         break;

      case 0x1F: // hvcurveto
         if (sp < 4) return _CSERR("hvcurveto stack");
         goto hvcurveto;
      case 0x1E: // vhcurveto
         if (sp < 4) return _CSERR("vhcurveto stack");
         for (;;) {
            if (i + 3 >= sp) break;
            _csctx_rccurve_to(c, 0, s[i], s[i+1], s[i+2], s[i+3], (sp - i == 5) ? s[i + 4] : 0.0f);
            i += 4;
      hvcurveto:
            if (i + 3 >= sp) break;
            _csctx_rccurve_to(c, s[i], 0, s[i+1], s[i+2], (sp - i == 5) ? s[i+4] : 0.0f, s[i+3]);
            i += 4;
         }
         break;

      case 0x08: // rrcurveto
         if (sp < 6) return _CSERR("rcurveline stack");
         for (; i + 5 < sp; i += 6)
            _csctx_rccurve_to(c, s[i], s[i+1], s[i+2], s[i+3], s[i+4], s[i+5]);
         break;

      case 0x18: // rcurveline
         if (sp < 8) return _CSERR("rcurveline stack");
         for (; i + 5 < sp - 2; i += 6)
            _csctx_rccurve_to(c, s[i], s[i+1], s[i+2], s[i+3], s[i+4], s[i+5]);
         if (i + 1 >= sp) return _CSERR("rcurveline stack");
         _csctx_rline_to(c, s[i], s[i+1]);
         break;

      case 0x19: // rlinecurve
         if (sp < 8) return _CSERR("rlinecurve stack");
         for (; i + 1 < sp - 6; i += 2)
            _csctx_rline_to(c, s[i], s[i+1]);
         if (i + 5 >= sp) return _CSERR("rlinecurve stack");
         _csctx_rccurve_to(c, s[i], s[i+1], s[i+2], s[i+3], s[i+4], s[i+5]);
         break;

      case 0x1A: // vvcurveto
      case 0x1B: // hhcurveto
         if (sp < 4) return _CSERR("(vv|hh)curveto stack");
         f = 0.0;
         if (sp & 1) { f = s[i]; i++; }
         for (; i + 3 < sp; i += 4) {
            if (b0 == 0x1B)
               _csctx_rccurve_to(c, s[i], f, s[i+1], s[i+2], s[i+3], 0.0);
            else
               _csctx_rccurve_to(c, f, s[i], s[i+1], s[i+2], 0.0, s[i+3]);
            f = 0.0;
         }
         break;

      case 0x0A: // callsubr
         if (!has_subrs) {
            if (info->fdselect.size)
               subrs = _cid_get_glyph_subrs(info, glyph_index);
            has_subrs = 1;
         }
         // FALLTHROUGH
      case 0x1D: // callgsubr
         if (sp < 1) return _CSERR("call(g|)subr stack");
         v = (int) s[--sp];
         if (subr_stack_height >= 10) return _CSERR("recursion limit");
         subr_stack[subr_stack_height++] = b;
         b = _get_subr(b0 == 0x0A ? subrs : info->gsubrs, v);
         if (b.size == 0) return _CSERR("subr not found");
         b.cursor = 0;
         clear_stack = 0;
         break;

      case 0x0B: // return
         if (subr_stack_height <= 0) return _CSERR("return outside subr");
         b = subr_stack[--subr_stack_height];
         clear_stack = 0;
         break;

      case 0x0E: // endchar
         _csctx_close_shape(c);
         return 1;

      case 0x0C: { // two-byte escape
         float dx1, dx2, dx3, dx4, dx5, dx6, dy1, dy2, dy3, dy4, dy5, dy6;
         float dx, dy;
         int b1 = buf_get8(&b);
         switch (b1) {
         // @TODO These "flex" implementations ignore the flex-depth and resolution,
         // and always draw beziers.
         case 0x22: // hflex
            if (sp < 7) return _CSERR("hflex stack");
            dx1 = s[0];
            dx2 = s[1];
            dy2 = s[2];
            dx3 = s[3];
            dx4 = s[4];
            dx5 = s[5];
            dx6 = s[6];
            _csctx_rccurve_to(c, dx1, 0, dx2, dy2, dx3, 0);
            _csctx_rccurve_to(c, dx4, 0, dx5, -dy2, dx6, 0);
            break;

         case 0x23: // flex
            if (sp < 13) return _CSERR("flex stack");
            dx1 = s[0];
            dy1 = s[1];
            dx2 = s[2];
            dy2 = s[3];
            dx3 = s[4];
            dy3 = s[5];
            dx4 = s[6];
            dy4 = s[7];
            dx5 = s[8];
            dy5 = s[9];
            dx6 = s[10];
            dy6 = s[11];
            //fd is s[12]
            _csctx_rccurve_to(c, dx1, dy1, dx2, dy2, dx3, dy3);
            _csctx_rccurve_to(c, dx4, dy4, dx5, dy5, dx6, dy6);
            break;

         case 0x24: // hflex1
            if (sp < 9) return _CSERR("hflex1 stack");
            dx1 = s[0];
            dy1 = s[1];
            dx2 = s[2];
            dy2 = s[3];
            dx3 = s[4];
            dx4 = s[5];
            dx5 = s[6];
            dy5 = s[7];
            dx6 = s[8];
            _csctx_rccurve_to(c, dx1, dy1, dx2, dy2, dx3, 0);
            _csctx_rccurve_to(c, dx4, 0, dx5, dy5, dx6, -(dy1+dy2+dy5));
            break;

         case 0x25: // flex1
            if (sp < 11) return _CSERR("flex1 stack");
            dx1 = s[0];
            dy1 = s[1];
            dx2 = s[2];
            dy2 = s[3];
            dx3 = s[4];
            dy3 = s[5];
            dx4 = s[6];
            dy4 = s[7];
            dx5 = s[8];
            dy5 = s[9];
            dx6 = dy6 = s[10];
            dx = dx1+dx2+dx3+dx4+dx5;
            dy = dy1+dy2+dy3+dy4+dy5;
            if (fabs(dx) > fabs(dy))
               dy6 = -dy;
            else
               dx6 = -dx;
            _csctx_rccurve_to(c, dx1, dy1, dx2, dy2, dx3, dy3);
            _csctx_rccurve_to(c, dx4, dy4, dx5, dy5, dx6, dy6);
            break;

         default:
            return _CSERR("unimplemented");
         }
      } break;

      default:
         if (b0 != 255 && b0 != 28 && b0 < 32)
            return _CSERR("reserved operator");

         // push immediate
         if (b0 == 255) {
            f = (float)(int32_t)buf_get32(&b) / 0x10000;
         } else {
            buf_skip(&b, -1);
            f = (float)(int16_t)_cff_int(&b);
         }
         if (sp >= 48) return _CSERR("push stack overflow");
         s[sp++] = f;
         clear_stack = 0;
         break;
      }
      if (clear_stack) sp = 0;
   }
   return _CSERR("no endchar");

#undef _CSERR
}

static int glyph_shape_T2(const fontinfo *info, int glyph_index, vertex **pvertices)
{
   // runs the charstring twice, once to count and once to output (to avoid realloc)
   _csctx count_ctx = _CSCTX_INIT(1);
   _csctx output_ctx = _CSCTX_INIT(0);
   if (_run_charstring(info, glyph_index, &count_ctx)) {
      *pvertices = (vertex*)malloc(count_ctx.num_vertices*sizeof(vertex));
      output_ctx.pvertices = *pvertices;
      if (_run_charstring(info, glyph_index, &output_ctx)) {
         assert(output_ctx.num_vertices == count_ctx.num_vertices);
         return output_ctx.num_vertices;
      }
   }
   *pvertices = NULL;
   return 0;
}

static int _glyph_info_T2(const fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1)
{
   _csctx c = _CSCTX_INIT(1);
   int r = _run_charstring(info, glyph_index, &c);
   if (x0)  *x0 = r ? c.min_x : 0;
   if (y0)  *y0 = r ? c.min_y : 0;
   if (x1)  *x1 = r ? c.max_x : 0;
   if (y1)  *y1 = r ? c.max_y : 0;
   return r ? c.num_vertices : 0;
}

static int glyph_shape(const fontinfo *info, int glyph_index, vertex **pvertices)
{
   if (!info->cff.size)
      return glyph_shape_TT(info, glyph_index, pvertices);
   else
      return glyph_shape_T2(info, glyph_index, pvertices);
}

void font_hmetrics(const fontinfo *info, int glyph_index, int *advanceWidth, int *leftSideBearing)
{
   uint16_t numOfLongHorMetrics = ttUSHORT(info->data+info->hhea + 34);
   if (glyph_index < numOfLongHorMetrics) {
      if (advanceWidth)     *advanceWidth    = ttSHORT(info->data + info->hmtx + 4*glyph_index);
      if (leftSideBearing)  *leftSideBearing = ttSHORT(info->data + info->hmtx + 4*glyph_index + 2);
   } else {
      if (advanceWidth)     *advanceWidth    = ttSHORT(info->data + info->hmtx + 4*(numOfLongHorMetrics-1));
      if (leftSideBearing)  *leftSideBearing = ttSHORT(info->data + info->hmtx + 4*numOfLongHorMetrics + 2*(glyph_index - numOfLongHorMetrics));
   }
}

int font_kerning_table_len(const fontinfo *info)
{
   uint8_t *data = info->data + info->kern;

   // we only look at the first table. it must be 'horizontal' and format 0.
   if (!info->kern)
      return 0;
   if (ttUSHORT(data+2) < 1) // number of tables, need at least 1
      return 0;
   if (ttUSHORT(data+8) != 1) // horizontal flag must be set in format
      return 0;

   return ttUSHORT(data+10);
}

int font_kerning_table(const fontinfo *info, kerningentry* table, int table_length)
{
   uint8_t *data = info->data + info->kern;
   int k, length;

   // we only look at the first table. it must be 'horizontal' and format 0.
   if (!info->kern)
      return 0;
   if (ttUSHORT(data+2) < 1) // number of tables, need at least 1
      return 0;
   if (ttUSHORT(data+8) != 1) // horizontal flag must be set in format
      return 0;

   length = ttUSHORT(data+10);
   if (table_length < length)
      length = table_length;

   for (k = 0; k < length; k++)
   {
      table[k].glyph1 = ttUSHORT(data+18+(k*6));
      table[k].glyph2 = ttUSHORT(data+20+(k*6));
      table[k].advance = ttSHORT(data+22+(k*6));
   }

   return length;
}

static int _glyph_kerning(const fontinfo *info, int glyph1, int glyph2)
{
   uint8_t *data = info->data + info->kern;
   uint32_t needle, straw;
   int l, r, m;

   // we only look at the first table. it must be 'horizontal' and format 0.
   if (!info->kern)
      return 0;
   if (ttUSHORT(data+2) < 1) // number of tables, need at least 1
      return 0;
   if (ttUSHORT(data+8) != 1) // horizontal flag must be set in format
      return 0;

   l = 0;
   r = ttUSHORT(data+10) - 1;
   needle = glyph1 << 16 | glyph2;
   while (l <= r) {
      m = (l + r) >> 1;
      straw = ttULONG(data+18+(m*6)); // note: unaligned read
      if (needle < straw)
         r = m - 1;
      else if (needle > straw)
         l = m + 1;
      else
         return ttSHORT(data+22+(m*6));
   }
   return 0;
}

static int32_t _coverage_index(uint8_t *coverageTable, int glyph)
{
   uint16_t coverageFormat = ttUSHORT(coverageTable);
   switch (coverageFormat) {
      case 1: {
         uint16_t glyphCount = ttUSHORT(coverageTable + 2);

         // Binary search.
         int32_t l=0, r=glyphCount-1, m;
         int straw, needle=glyph;
         while (l <= r) {
            uint8_t *glyphArray = coverageTable + 4;
            uint16_t glyphID;
            m = (l + r) >> 1;
            glyphID = ttUSHORT(glyphArray + 2 * m);
            straw = glyphID;
            if (needle < straw)
               r = m - 1;
            else if (needle > straw)
               l = m + 1;
            else {
               return m;
            }
         }
         break;
      }

      case 2: {
         uint16_t rangeCount = ttUSHORT(coverageTable + 2);
         uint8_t *rangeArray = coverageTable + 4;

         // Binary search.
         int32_t l=0, r=rangeCount-1, m;
         int strawStart, strawEnd, needle=glyph;
         while (l <= r) {
            uint8_t *rangeRecord;
            m = (l + r) >> 1;
            rangeRecord = rangeArray + 6 * m;
            strawStart = ttUSHORT(rangeRecord);
            strawEnd = ttUSHORT(rangeRecord + 2);
            if (needle < strawStart)
               r = m - 1;
            else if (needle > strawEnd)
               l = m + 1;
            else {
               uint16_t startCoverageIndex = ttUSHORT(rangeRecord + 4);
               return startCoverageIndex + glyph - strawStart;
            }
         }
         break;
      }

      default: return -1; // unsupported
   }

   return -1;
}

static int32_t _glyph_class(uint8_t *classDefTable, int glyph)
{
   uint16_t classDefFormat = ttUSHORT(classDefTable);
   switch (classDefFormat)
   {
      case 1: {
         uint16_t startGlyphID = ttUSHORT(classDefTable + 2);
         uint16_t glyphCount = ttUSHORT(classDefTable + 4);
         uint8_t *classDef1ValueArray = classDefTable + 6;

         if (glyph >= startGlyphID && glyph < startGlyphID + glyphCount)
            return (int32_t)ttUSHORT(classDef1ValueArray + 2 * (glyph - startGlyphID));
         break;
      }

      case 2: {
         uint16_t classRangeCount = ttUSHORT(classDefTable + 2);
         uint8_t *classRangeRecords = classDefTable + 4;

         // Binary search.
         int32_t l=0, r=classRangeCount-1, m;
         int strawStart, strawEnd, needle=glyph;
         while (l <= r) {
            uint8_t *classRangeRecord;
            m = (l + r) >> 1;
            classRangeRecord = classRangeRecords + 6 * m;
            strawStart = ttUSHORT(classRangeRecord);
            strawEnd = ttUSHORT(classRangeRecord + 2);
            if (needle < strawStart)
               r = m - 1;
            else if (needle > strawEnd)
               l = m + 1;
            else
               return (int32_t)ttUSHORT(classRangeRecord + 4);
         }
         break;
      }

      default:
         return -1; // Unsupported definition type, return an error.
   }

   // "All glyphs not assigned to a class fall into class 0". (OpenType spec)
   return 0;
}

// Define to assert(x) if you want to break on unimplemented formats.
#define GPOS_TODO_assert(x)

static int32_t _glyph_GPOS_advance(const fontinfo *info, int glyph1, int glyph2)
{
   uint16_t lookupListOffset;
   uint8_t *lookupList;
   uint16_t lookupCount;
   uint8_t *data;
   int32_t i, sti;

   if (!info->gpos) return 0;

   data = info->data + info->gpos;

   if (ttUSHORT(data+0) != 1) return 0; // Major version 1
   if (ttUSHORT(data+2) != 0) return 0; // Minor version 0

   lookupListOffset = ttUSHORT(data+8);
   lookupList = data + lookupListOffset;
   lookupCount = ttUSHORT(lookupList);

   for (i=0; i<lookupCount; ++i) {
      uint16_t lookupOffset = ttUSHORT(lookupList + 2 + 2 * i);
      uint8_t *lookupTable = lookupList + lookupOffset;

      uint16_t lookupType = ttUSHORT(lookupTable);
      uint16_t subTableCount = ttUSHORT(lookupTable + 4);
      uint8_t *subTableOffsets = lookupTable + 6;
      if (lookupType != 2) // Pair Adjustment Positioning Subtable
         continue;

      for (sti=0; sti<subTableCount; sti++) {
         uint16_t subtableOffset = ttUSHORT(subTableOffsets + 2 * sti);
         uint8_t *table = lookupTable + subtableOffset;
         uint16_t posFormat = ttUSHORT(table);
         uint16_t coverageOffset = ttUSHORT(table + 2);
         int32_t coverageIndex = _coverage_index(table + coverageOffset, glyph1);
         if (coverageIndex == -1) continue;

         switch (posFormat) {
            case 1: {
               int32_t l, r, m;
               int straw, needle;
               uint16_t valueFormat1 = ttUSHORT(table + 4);
               uint16_t valueFormat2 = ttUSHORT(table + 6);
               if (valueFormat1 == 4 && valueFormat2 == 0) { // Support more formats?
                  int32_t valueRecordPairSizeInBytes = 2;
                  uint16_t pairSetCount = ttUSHORT(table + 8);
                  uint16_t pairPosOffset = ttUSHORT(table + 10 + 2 * coverageIndex);
                  uint8_t *pairValueTable = table + pairPosOffset;
                  uint16_t pairValueCount = ttUSHORT(pairValueTable);
                  uint8_t *pairValueArray = pairValueTable + 2;

                  if (coverageIndex >= pairSetCount) return 0;

                  needle=glyph2;
                  r=pairValueCount-1;
                  l=0;

                  // Binary search.
                  while (l <= r) {
                     uint16_t secondGlyph;
                     uint8_t *pairValue;
                     m = (l + r) >> 1;
                     pairValue = pairValueArray + (2 + valueRecordPairSizeInBytes) * m;
                     secondGlyph = ttUSHORT(pairValue);
                     straw = secondGlyph;
                     if (needle < straw)
                        r = m - 1;
                     else if (needle > straw)
                        l = m + 1;
                     else {
                        int16_t xAdvance = ttSHORT(pairValue + 2);
                        return xAdvance;
                     }
                  }
               } else
                  return 0;
               break;
            }

            case 2: {
               uint16_t valueFormat1 = ttUSHORT(table + 4);
               uint16_t valueFormat2 = ttUSHORT(table + 6);
               if (valueFormat1 == 4 && valueFormat2 == 0) { // Support more formats?
                  uint16_t classDef1Offset = ttUSHORT(table + 8);
                  uint16_t classDef2Offset = ttUSHORT(table + 10);
                  int glyph1class = _glyph_class(table + classDef1Offset, glyph1);
                  int glyph2class = _glyph_class(table + classDef2Offset, glyph2);

                  uint16_t class1Count = ttUSHORT(table + 12);
                  uint16_t class2Count = ttUSHORT(table + 14);
                  uint8_t *class1Records, *class2Records;
                  int16_t xAdvance;

                  if (glyph1class < 0 || glyph1class >= class1Count) return 0; // malformed
                  if (glyph2class < 0 || glyph2class >= class2Count) return 0; // malformed

                  class1Records = table + 16;
                  class2Records = class1Records + 2 * (glyph1class * class2Count);
                  xAdvance = ttSHORT(class2Records + 2 * glyph2class);
                  return xAdvance;
               } else
                  return 0;
               break;
            }

            default:
               return 0; // Unsupported position format
         }
      }
   }

   return 0;
}

int font_kerning(const fontinfo *info, int g1, int g2)
{
   int xAdvance = 0;

   if (info->gpos)
      xAdvance += _glyph_GPOS_advance(info, g1, g2);
   else if (info->kern)
      xAdvance += _glyph_kerning(info, g1, g2);

   return xAdvance;
}

void font_vmetrics(const fontinfo *info, int *ascent, int *descent, int *lineGap)
{
   if (ascent ) *ascent  = ttSHORT(info->data+info->hhea + 4);
   if (descent) *descent = ttSHORT(info->data+info->hhea + 6);
   if (lineGap) *lineGap = ttSHORT(info->data+info->hhea + 8);
}

int font_vmetrics_OS2(const fontinfo *info, int *typoAscent, int *typoDescent, int *typoLineGap)
{
   int tab = _find_table(info->data, info->fontstart, "OS/2");
   if (!tab)
      return 0;
   if (typoAscent ) *typoAscent  = ttSHORT(info->data+tab + 68);
   if (typoDescent) *typoDescent = ttSHORT(info->data+tab + 70);
   if (typoLineGap) *typoLineGap = ttSHORT(info->data+tab + 72);
   return 1;
}

void font_bounding_box(const fontinfo *info, int *x0, int *y0, int *x1, int *y1)
{
   *x0 = ttSHORT(info->data + info->head + 36);
   *y0 = ttSHORT(info->data + info->head + 38);
   *x1 = ttSHORT(info->data + info->head + 40);
   *y1 = ttSHORT(info->data + info->head + 42);
}

float font_scale(const fontinfo *info, float pt, float ppi)
{
   int unitsPerEm = ttUSHORT(info->data + info->head + 18);
   return (pt * 72) / (unitsPerEm * ppi);
}

//////////////////////////////////////////////////////////////////////////////
//
//  Rasterizer
//

void font_bitmap_box(const fontinfo *info, int glyph, float scale_x, float scale_y, float shift_x, float shift_y, int *x_min, int *width, int *height, float *baseline) {
   int x0, y0, x1, y1;
   if (!GetGlyphBox(info, glyph, &x0, &y0, &x1, &y1)) { // e.g. space character
      *width = *height = *baseline = 0;
   } else {
      // move glyph horizontally to left edge and shift baseline downwards by shift_y.
      // This means there will be a pixel boundary 'shift_y' pixels above the glyph's baseline (y=0 in font-units)
      *x_min = x0;
      *width = iceil((x1 - x0) * scale_x + shift_x);
      *height = iceil(y1 * scale_y - shift_y) + iceil(-y0 * scale_y + shift_y);
      *baseline = iceil(y1 * scale_y - shift_y) + shift_y;
   }
}

typedef struct Root {
   double x;
   bool rising;
} Root;

sb_declare(Root);

// sort roots from low to high
static int root_cmp(const void *a, const void *b) {
   const Root *ra = a, *rb = b;
   return (ra->x > rb->x) - (ra->x < rb->x);
}

static void linear_root(int x0, int x1, int y0, int y1, float y_off, sb_Root *roots) {
   if ((y0 - y_off > 0 && y1 - y_off <= 0) || (y0 - y_off <= 0 && y1 - y_off > 0)) {
      double t = -(y0 - y_off) / (y1 - y0);
      double x = (x1 - x0) * t + x0;
      sb_append(roots, (Root) {.x = x, .rising = y0 < y1});
   }
}

static void quadratic_root(int x0, int x1, int x2, int y0, int y1, int y2, float y_off, sb_Root *roots) {
   int eligible = 0x2E74 >> (((y0 - y_off) > 0 ? 2 : 0) + ((y1 - y_off) > 0 ? 4 : 0) + ((y2 - y_off) > 0 ? 8 : 0));
   if (eligible & 3) {
      double a = y0 - 2*y1 + y2, b = y0 - y1, c = y0 - y_off;
      double t0, t1;
      if (a == 0) t0 = t1 = c / (2*b);
      else {
         double _r = sqrt(max(0, b*b - a*c));
         t0 = (b - _r) / a;
         t1 = (b + _r) / a;
      }
      a = x0 - 2*x1 + x2, b = -2*(x0 - x1), c = x0;
      if (eligible & 1) {
         double x = a*t0*t0 + b*t0 + c;
         sb_append(roots, (Root) {.x = x, .rising = false});
      }
      if (eligible & 2) {
         double x = a*t1*t1 + b*t1 + c;
         sb_append(roots, (Root) {.x = x, .rising = true});
      }
   }
}

typedef struct _bitmap
{
   int w,h;
   float baseline;
   unsigned char *pixels;
} _bitmap;

static void tt_rasterize(vertex *vertices, int num_verts, float scale_x, float scale_y, float shift_x, float shift_y, int x_min, int sample_count, _bitmap *result) {
   ensure(result != NULL);
   ensure(sample_count > 0 && sample_count <= 257); // 257+ samples overflow uint16 accumulator if it only accumulates max values (255)
   sb_Root roots = sb_create(Root, num_verts); // estimating number of roots to be less or equal than num_verts
   uint16_t *accumulator = malloc(max(result->h, result->w) * sizeof(uint16_t));
   ensure(accumulator != NULL);

   // HORIZONTAL RAY (going from top to bottom and left to right)
   for (int y = 0; y < result->h; y++) {
      for (int x = 0; x < result->w; x++) accumulator[x] = 0;
      for (int sy = 0; sy < sample_count; sy++) {
         float y_off = (result->baseline - y - (sy + 0.5)/sample_count)/scale_y; // ray y in font-units in tt coordinate system
         // find all roots
         int last_x, last_y;
         for (int i = 0; i < num_verts; i++) {
            vertex vert = vertices[i];
            if (i == 0) ensure(vert.type == vmove);
            switch (vert.type) {
               case vline:
                  linear_root(last_x, vert.x, last_y, vert.y, y_off, &roots);
                  break;
               case vcurve:
                  quadratic_root(last_x, vert.cx, vert.x, last_y, vert.cy, vert.y, y_off, &roots);
                  break;
               case vcubic:
                  assert(0);
            }
            last_x = vert.x;
            last_y = vert.y;
         }
         if (roots.count == 0) continue;
         qsort(roots.items, roots.count, sizeof(Root), root_cmp);
         int winding_number_total = 0;
         size_t i = 0;
         for (int x = 0; x < result->w; x++) {
            float winding_number = winding_number_total;
            for (; i < roots.count; i++) {
               Root r = sb_get(roots, i);
               // distance from intersection point to sampling position. x+1 is sampling position.
               // Subtrahend is intersection point in pixels relative to left most point of character
               float rx = (x + 1) - (r.x - x_min)*scale_x + shift_x; 
               if (rx > 0) {
                  winding_number += (r.rising ? 1 : -1) * min(1, rx);
                  winding_number_total += r.rising ? 1 : -1;
               } else break;
            }
            accumulator[x] += ifloor(min(1, fabs(winding_number)) * 255 +.5);
         }
         roots.count = 0;
      }
      for (int x = 0; x < result->w; x++) result->pixels[y * result->w + x] = ifloor(accumulator[x]/(sample_count) + 0.5);
   }

   // VERTICAL RAY (from left to right and top to bottom)
   for (int x = 0; x < result->w; x++) {
      for (int y = 0; y < result->h; y++) accumulator[y] = 0;
      for (int sx = 0; sx < sample_count; sx++) {
         float x_off = (x + (sx + 0.5)/sample_count)/scale_x + x_min; // ray x in font-units in tt coordinate system
         // find all roots
         int last_x, last_y;
         for (int i = 0; i < num_verts; i++) {
            vertex vert = vertices[i];
            if (i == 0) ensure(vert.type == vmove);
            switch (vert.type) {
               case vline:
                  linear_root(last_y, vert.y, last_x, vert.x, x_off, &roots);
                  break;
               case vcurve:
                  quadratic_root(last_y, vert.cy, vert.y, last_x, vert.cx, vert.x, x_off, &roots);
                  break;
               case vcubic:
                  assert(0);
            }
            last_x = vert.x;
            last_y = vert.y;
         }
         if (roots.count == 0) continue;
         qsort(roots.items, roots.count, sizeof(Root), root_cmp);
         size_t i = 0;
         int winding_number_total = 0;
         for (int y = result->h - 1; y >= 0; y--) {
            float winding_number = winding_number_total;
            for (; i < roots.count; i++) {
               Root r = sb_get(roots, i);
               // // distance from intersection point to sampling position. result->baseline - y - 1 is sampling position.
               // // Subtrahend is intersection point in pixels relative to baseline
               float ry = (result->baseline - y) - (r.x*scale_y - shift_y);
               if (ry > 0) {
                  winding_number += (r.rising ? 1 : -1) * min(1, fabs(ry));
                  winding_number_total += r.rising ? 1 : -1;
               } else break;
            }
            accumulator[y] += ifloor(min(1, fabs(winding_number)) * 255 +.5);
         }
         roots.count = 0;
      }
      for (int y = 0; y < result->h; y++) result->pixels[y * result->w + x] = ((uint16_t) result->pixels[y * result->w + x] + ifloor(accumulator[y]/(sample_count) + 0.5)) / 2;
   }
   free(accumulator);
   sb_destroy(&roots);
}

unsigned char *font_bitmap(const fontinfo *info, float scale_x, float scale_y, float shift_x, float shift_y, int glyph, int samples, int *width, int *height, float *baseline)
{
   if (samples <= 0 || scale_x == 0 || scale_y == 0) return NULL;
   
   _bitmap gbm;
   vertex *vertices;
   int x_min; // font-units of left-most vertex
   int num_verts = glyph_shape(info, glyph, &vertices);

   font_bitmap_box(info, glyph, scale_x, scale_y, shift_x, shift_y, &x_min, &gbm.w, &gbm.h, &gbm.baseline);
   gbm.pixels = NULL; // in case we error

   if (width ) *width  = gbm.w;
   if (height) *height = gbm.h;
   if (baseline) *baseline = gbm.baseline;

   if (gbm.w && gbm.h) {
      gbm.pixels = (unsigned char *) malloc(gbm.w * gbm.h);
      if (gbm.pixels) tt_rasterize(vertices, num_verts, scale_x, scale_y, shift_x, shift_y, x_min, samples, &gbm);
   }
   free(vertices);
   return gbm.pixels;
}


//////////////////////////////////////////////////////////////////////////////
//
// font name matching -- recommended not to use this
//

// check if a utf8 string contains a prefix which is the utf16 string; if so return length of matching utf8 string
static int32_t _utf8_eq_utf16_bigendian_prefix(uint8_t *s1, int32_t len1, uint8_t *s2, int32_t len2)
{
   int32_t i=0;

   // convert utf16 to utf8 and compare the results while converting
   while (len2) {
      uint16_t ch = s2[0]*256 + s2[1];
      if (ch < 0x80) {
         if (i >= len1) return -1;
         if (s1[i++] != ch) return -1;
      } else if (ch < 0x800) {
         if (i+1 >= len1) return -1;
         if (s1[i++] != 0xc0 + (ch >> 6)) return -1;
         if (s1[i++] != 0x80 + (ch & 0x3f)) return -1;
      } else if (ch >= 0xd800 && ch < 0xdc00) {
         uint32_t c;
         uint16_t ch2 = s2[2]*256 + s2[3];
         if (i+3 >= len1) return -1;
         c = ((ch - 0xd800) << 10) + (ch2 - 0xdc00) + 0x10000;
         if (s1[i++] != 0xf0 + (c >> 18)) return -1;
         if (s1[i++] != 0x80 + ((c >> 12) & 0x3f)) return -1;
         if (s1[i++] != 0x80 + ((c >>  6) & 0x3f)) return -1;
         if (s1[i++] != 0x80 + ((c      ) & 0x3f)) return -1;
         s2 += 2; // plus another 2 below
         len2 -= 2;
      } else if (ch >= 0xdc00 && ch < 0xe000) {
         return -1;
      } else {
         if (i+2 >= len1) return -1;
         if (s1[i++] != 0xe0 + (ch >> 12)) return -1;
         if (s1[i++] != 0x80 + ((ch >> 6) & 0x3f)) return -1;
         if (s1[i++] != 0x80 + ((ch     ) & 0x3f)) return -1;
      }
      s2 += 2;
      len2 -= 2;
   }
   return i;
}

static int utf8_eq_utf16_bigendian_internal(char *s1, int len1, char *s2, int len2)
{
   return len1 == _utf8_eq_utf16_bigendian_prefix((uint8_t*) s1, len1, (uint8_t*) s2, len2);
}

// returns results in whatever encoding you request... but note that 2-byte encodings
// will be BIG-ENDIAN... use utf8_eq_utf16_bigendian() to compare
const char *font_name(const fontinfo *font, int *length, int platformID, int encodingID, int languageID, int nameID)
{
   int32_t i,count,stringOffset;
   uint8_t *fc = font->data;
   uint32_t offset = font->fontstart;
   uint32_t nm = _find_table(fc, offset, "name");
   if (!nm) return NULL;

   count = ttUSHORT(fc+nm+2);
   stringOffset = nm + ttUSHORT(fc+nm+4);
   for (i=0; i < count; ++i) {
      uint32_t loc = nm + 6 + 12 * i;
      if (platformID == ttUSHORT(fc+loc+0) && encodingID == ttUSHORT(fc+loc+2)
          && languageID == ttUSHORT(fc+loc+4) && nameID == ttUSHORT(fc+loc+6)) {
         *length = ttUSHORT(fc+loc+8);
         return (const char *) (fc+stringOffset+ttUSHORT(fc+loc+10));
      }
   }
   return NULL;
}

static int _matchpair(uint8_t *fc, uint32_t nm, uint8_t *name, int32_t nlen, int32_t target_id, int32_t next_id)
{
   int32_t i;
   int32_t count = ttUSHORT(fc+nm+2);
   int32_t stringOffset = nm + ttUSHORT(fc+nm+4);

   for (i=0; i < count; ++i) {
      uint32_t loc = nm + 6 + 12 * i;
      int32_t id = ttUSHORT(fc+loc+6);
      if (id == target_id) {
         // find the encoding
         int32_t platform = ttUSHORT(fc+loc+0), encoding = ttUSHORT(fc+loc+2), language = ttUSHORT(fc+loc+4);

         // is this a Unicode encoding?
         if (platform == 0 || (platform == 3 && encoding == 1) || (platform == 3 && encoding == 10)) {
            int32_t slen = ttUSHORT(fc+loc+8);
            int32_t off = ttUSHORT(fc+loc+10);

            // check if there's a prefix match
            int32_t matchlen = _utf8_eq_utf16_bigendian_prefix(name, nlen, fc+stringOffset+off,slen);
            if (matchlen >= 0) {
               // check for target_id+1 immediately following, with same encoding & language
               if (i+1 < count && ttUSHORT(fc+loc+12+6) == next_id && ttUSHORT(fc+loc+12) == platform && ttUSHORT(fc+loc+12+2) == encoding && ttUSHORT(fc+loc+12+4) == language) {
                  slen = ttUSHORT(fc+loc+12+8);
                  off = ttUSHORT(fc+loc+12+10);
                  if (slen == 0) {
                     if (matchlen == nlen)
                        return 1;
                  } else if (matchlen < nlen && name[matchlen] == ' ') {
                     ++matchlen;
                     if (utf8_eq_utf16_bigendian_internal((char*) (name+matchlen), nlen-matchlen, (char*)(fc+stringOffset+off),slen))
                        return 1;
                  }
               } else {
                  // if nothing immediately following
                  if (matchlen == nlen)
                     return 1;
               }
            }
         }

         // @TODO handle other encodings
      }
   }
   return 0;
}

static int _matches(uint8_t *fc, uint32_t offset, uint8_t *name, int32_t flags)
{
   int32_t nlen = (int32_t) strlen((char *) name);
   uint32_t nm,hd;
   if (!_isfont(fc+offset)) return 0;

   // check italics/bold/underline flags in macStyle...
   if (flags) {
      hd = _find_table(fc, offset, "head");
      if ((ttUSHORT(fc+hd+44) & 7) != (flags & 7)) return 0;
   }

   nm = _find_table(fc, offset, "name");
   if (!nm) return 0;

   if (flags) {
      // if we checked the macStyle flags, then just check the family and ignore the subfamily
      if (_matchpair(fc, nm, name, nlen, 16, -1))  return 1;
      if (_matchpair(fc, nm, name, nlen,  1, -1))  return 1;
      if (_matchpair(fc, nm, name, nlen,  3, -1))  return 1;
   } else {
      if (_matchpair(fc, nm, name, nlen, 16, 17))  return 1;
      if (_matchpair(fc, nm, name, nlen,  1,  2))  return 1;
      if (_matchpair(fc, nm, name, nlen,  3, -1))  return 1;
   }

   return 0;
}

static int font_match_internal(unsigned char *font_collection, char *name_utf8, int32_t flags)
{
   int32_t i;
   for (i=0;;++i) {
      int32_t off = font_offset_for_index(font_collection, i);
      if (off < 0) return off;
      if (_matches((uint8_t *) font_collection, off, (uint8_t*) name_utf8, flags))
         return off;
   }
}

int font_offset_for_index(const unsigned char *data, int index)
{
   return font_offset_for_index_internal((unsigned char *) data, index);
}

int font_count(const unsigned char *data)
{
   return font_count_internal((unsigned char *) data);
}

fontinfo *font_load(const unsigned char *data, int offset)
{
   fontinfo *info = malloc(sizeof(fontinfo));
   if (info == NULL || font_init_internal(info, (unsigned char *) data, offset) == 0) return NULL;
   return info;
}

int font_match(const unsigned char *fontdata, const char *name, int flags)
{
   return font_match_internal((unsigned char *) fontdata, (char *) name, flags);
}

int utf8_eq_utf16_bigendian(const char *s1, int len1, const char *s2, int len2)
{
   return utf8_eq_utf16_bigendian_internal((char *) s1, len1, (char *) s2, len2);
}
