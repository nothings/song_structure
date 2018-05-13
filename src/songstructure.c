#define STB_DEFINE
#include "stb.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"


/*
   v ; verse
   c ; chorus
   i ; intro
   o ; outro
   p ; pre-chorus
   b ; bridge
   "section 1" ; defaults to zv
   zX ; color selector X is a letter a-z,A-Z

   s1/4

   g4
   4 4 4 4 4 4 4 4 4 4 4 3/4

   s1/4 ; subdivision
   s3/8

   "verse"
   "chorus"   
*/

typedef struct
{
   int numerator;
   int denominator;
} fraction;

// n should be a 3-digit hex number
#define rgb(n)   {   17*((n)>>8), 17*(((n)>>4)&15), 17*((n)&15)  }

uint8 white[3] = rgb(0xfff); // { 255,255,255};

int color_variant;
uint32 color_table[][11] =
{
   { { 'i' }, 0xA35, 0xA63, 0x838, 0xc48 },
   { { 'v' }, 0x39B, 0x14B, 0x549, 0x168, 0x68B, 0x639, 0x47D },
   { { 'c' }, 0xF26, 0xC96, 0xF98, 0xE55, 0xB55, 0xD48 },
   { { 'p' }, 0x33E, 0x51B, 0x94E, 0x55A },
   { { 'o' }, 0x665, 0x996, 0x974, }, 
   { { 'b' }, 0xC6F, 0xC8A, 0x78C, 0xE6C },
   { { 'a' }, 0x666, 0x999, 0x333, 0x363 }, 
   { { 'e' }, 0x586, 0x439, 0x637 },
};

#if 0
uint8 colors[1+26+26][3];
{
   { 255,255,255 },
   { 0, 0, 0 },
   { 229, 111, 254 },
   { 255, 0, 86 },  // c
   { 158, 0, 142 },
   { 14, 76, 161 }, // v2
   { 255, 229, 2 },
   { 0, 95, 57 },
   { 0, 255, 0 },
   { 149, 0, 58 },
   { 255, 147, 126 }, // c2
   { 164, 36, 0 },
   { 0, 21, 68 },
   { 145, 208, 203 },  // m
   { 98, 14, 0 },
   { 107, 104, 130 }, // v3
   { 0, 0, 255 },
   { 0, 125, 181 },
   { 106, 130, 108 },
   { 0, 174, 126 },
   { 194, 140, 159 },
   { 190, 153, 112 }, // c3
   { 0, 143, 156 }, // v
   { 95, 173, 78 },
   { 255, 0, 0 },
   { 255, 0, 246 },
   { 255, 2, 157 },
   { 104, 61, 59 },
   { 255, 116, 163 },
   { 150, 138, 232 },
   { 152, 255, 82 },
   { 167, 87, 64 },
   { 1, 255, 254 },
   { 255, 238, 232 },
   { 254, 137, 0 },
   { 189, 198, 255 },
   { 1, 208, 255 },
   { 187, 136, 0 },
   { 117, 68, 177 },
   { 165, 255, 210 },
   { 255, 166, 254 },
   { 119, 77, 0 },
   { 122, 71, 130 },
   { 38, 52, 0 },
   { 0, 71, 84 },
   { 67, 0, 44 },
   { 181, 0, 255 },
   { 255, 177, 103 },
   { 255, 219, 102 },
   { 144, 251, 146 },
   { 126, 45, 210 },
   { 189, 211, 147 },
   { 213, 255, 0 },
};
#endif

fraction beat_duration;
int section_color; // letter a-z A-Z, or post-decoded color?
int left_edge;
int cursor_x, cursor_y;
int max_beat_height;
int default_denominator;
float ghost = 0;

#define MEASURE_HORIZONTAL_SPACING    4
#define MEASURE_VERTICAL_SPACING      4

#define LINE_SPACING           (QUARTER_HEIGHT + MEASURE_VERTICAL_SPACING)
#define QUARTER_HEIGHT    10

int beat_width;


uint8 *pixels;
int out_w, out_h;
int max_x, max_y;

void plot_pixel(int x, int y, uint8 *color)
{
   if (x < 0 || y < 0)
      return;
   if (x >= out_w || y >= out_h) {
      int j;
      uint8 *new_image;
      int new_w = out_w, new_h = out_h;
      if (out_w == 0 || out_h == 0) {
         new_w = 1024;
         new_h = 2048;
      }
      if (x >= out_w)
         new_w = stb_max(x+10, new_w+200);
      if (y >= out_h)
         new_h = stb_max(y+10, new_h+200);

      new_image = malloc(new_w * new_h * 3);
      if (new_image == NULL)
         stb_fatal("Out of memory");
      memset(new_image, 255, new_w*new_h*3);
      for (j=0; j < out_h; ++j)
         memcpy(new_image + new_w*3*j, pixels + out_w*3*j, 3*out_w);
      free(pixels);
      pixels = new_image;
      out_w = new_w;
      out_h = new_h;
   }
   if (color)
      memcpy(pixels + out_w*3*y + 3*x, color, 3);
   max_x = stb_max(max_x, x);
   max_y = stb_max(max_y, y);
}

void plot_pixel_faded(int x, int y, uint8 *color, float a, unsigned char blend[3])
{
   int i;
   uint8 c[3];
   for (i=0; i < 3; ++i)
      c[i] = (uint8) stb_lerp(a, blend[i], color[i]);
   plot_pixel(x, y, c);
}

void plot_pixel_alpha(int x, int y, uint8 *color, float a)
{
   plot_pixel_faded(x, y, color, a, pixels+x*3+y*3*out_w);
}


void plot_hline(int x0, int x1, int y, unsigned char color[3], float a0, float a1)
{
   int x;
   for (x=x0; x < x1; ++x)
      plot_pixel_alpha(x,y,color,stb_linear_remap(x,x0,x1,a0,a1));
}

void plot_vline(int x, int y0, int y1, unsigned char color[3], float a0, float a1)
{
   int y;
   for (y=y0; y < y1; ++y)
      plot_pixel_alpha(x,y,color,stb_linear_remap(y,y0,y1,a0,a1));
}


stbtt_fontinfo font;
void draw_char(int codepoint, float scale, int xpos, int ypos, uint8 *color)
{
   int i,j,w,h,xoff,yoff;
   uint8 *bitmap;
   bitmap = stbtt_GetCodepointBitmap(&font, 0,scale, codepoint, &w, &h, &xoff,&yoff);
   for (j=0; j < h; ++j)
      for (i=0; i < w; ++i) {
         // force pixel to exist
         plot_pixel(xpos+i+xoff, ypos+j+yoff, NULL);
         plot_pixel_faded(xpos + i + xoff, ypos + j + yoff, color, bitmap[j*w+i]/255.0f, pixels+(xpos+i+xoff)*3+(ypos+j+yoff)*3*out_w);
      }
}

int draw_text(char *name, float size, int xpos, int y_top_pos, int r, int g, int b, int right_justify)
{
   int ypos,ascent;
   uint8 color[3] = { r,g,b };
   char *p;
   float scale;
   scale = stbtt_ScaleForPixelHeight(&font, size);
   stbtt_GetFontVMetrics(&font, &ascent, NULL, NULL);
   ypos = y_top_pos + (int) (ascent * scale);

   while (*name == ' ') ++name;

   if (right_justify) {
      p = name;
      while (*p) {
         int advance;
         stbtt_GetCodepointHMetrics(&font, *p, &advance, NULL);
         xpos -= (int) (advance * scale);
         ++p;
      }
   }

   p = name;
   while (*p) {
      int advance;
      draw_char(*p, scale, xpos, ypos, color);
      stbtt_GetCodepointHMetrics(&font, *p, &advance, NULL);
      xpos += (int) (advance * scale);
      ++p;
   }
   // if right_justify == 0, left-justify
   return y_top_pos + (int) size + 10;
}

void endline(void)
{
   cursor_x = left_edge;
   cursor_y += max_beat_height + MEASURE_VERTICAL_SPACING;
   max_beat_height = 0;
}

char *draw_text_line(char *text, float size, int r, int g, int b)
{
   char *p = text, c;
   while (*p && *p != '\n' && *p != '\r')
      ++p;
   c = *p;
   *p = 0;
   cursor_y = draw_text(text, size, cursor_x, cursor_y, r,g,b, 0);
   *p = c;
   return p;
}

char *text_line(char **text)
{
   char *start = *text;
   char *p = start;
   while (*p && *p != '\n' && *p != '\r')
      ++p;
   if (*p)
      *p++=0;
   *text = p;
   while (*start && *start == ' ')
      ++start;
   return start;
}


int pickup;
int delay;
int hide_measure; // like delay, but arbitrary measure spacing
float beat_vscale;
int bracket;

unsigned char internal_line_color[3] = { 255,255,255 };
unsigned char border_line_color[3] = { 255,255,255 };
unsigned char bracket_line_color[3] = { 0,0,0 };
//unsigned char internal_line_color[3] = { 0,0,0 };
//unsigned char border_line_color[3] = { 0,0,0 };

int prev_measure_left, prev_measure_right, prev_beat_height;
float bracket_alpha = 0.35f;
float bracket_alpha_end = 0.0f;
float bracket_start;
int spacing = 0;

//    (a/b)
//    -----  =>  (a/b) * (d/c)
//    (c/d)
int draw_measure(int x, int y, fraction m)
{
   uint8 c[3] = rgb(color_table[section_color][color_variant]);
   float alpha_for_height[256];
   int i,j,k, xpos=0;
   int   full_b;
   float beat_rescale = (float) beat_duration.numerator / beat_duration.denominator;
   int beat_height = (int) (QUARTER_HEIGHT * 4.0 * stb_linear_remap(beat_rescale, 0.25,0.33f, 0.25,0.30f) * beat_vscale);
   int BEAT_WIDTH = (beat_width * 4 * beat_duration.numerator / beat_duration.denominator);
   float frac_b;
   float beats = (            m.numerator   / (float)             m.denominator) *
                  (beat_duration.denominator / (float) beat_duration.numerator  )    ;
   full_b = (int) floor(beats);
   frac_b = beats - full_b;

   if (ghost) {
      c[0] = (int) stb_lerp(ghost, c[0], 255);
      c[1] = (int) stb_lerp(ghost, c[1], 255);
      c[2] = (int) stb_lerp(ghost, c[2], 255);
   }
   if (pickup) {
      xpos -= BEAT_WIDTH * (full_b + (frac_b ? 1 : 0)) + MEASURE_HORIZONTAL_SPACING + 1;
      pickup = 0;
   }
   if (delay) {
      // skip space for extra beats at beginning -- like a pickup, but not left-aligned
      xpos += BEAT_WIDTH * delay;
   }

   if (bracket) {
      // if bracket is 1, we need to draw the left border as well
      if (bracket == 1) {
         //xpos += 2;
         //plot_vline(cursor_x+xpos, cursor_y-1, cursor_y+1, bracket_line_color, bracket_start, bracket_alpha);
         //plot_vline(cursor_x+xpos, cursor_y+beat_height-1, cursor_y+beat_height+1, bracket_line_color, bracket_start, bracket_alpha);
         //plot_vline(cursor_x+xpos-2, cursor_y-2, cursor_y+beat_height+2, bracket_line_color, bracket_alpha);
         //plot_hline(cursor_x+xpos-2, cursor_x, cursor_y-2, bracket_line_color, bracket_alpha);
         //plot_hline(cursor_x+xpos-2, cursor_x, cursor_y+beat_height+1, bracket_line_color, bracket_alpha);
      } else {
         //plot_hline(prev_measure_right, cursor_x, cursor_y+1*beat_height/5, bracket_line_color, bracket_alpha, bracket_alpha);
         //plot_hline(prev_measure_right, cursor_x, cursor_y+4*beat_height/5-1, bracket_line_color, bracket_alpha, bracket_alpha);
         plot_hline(prev_measure_left, cursor_x, cursor_y-spacing, bracket_line_color, bracket_start, bracket_alpha);
         plot_hline(prev_measure_left, cursor_x, cursor_y+beat_height-1+spacing, bracket_line_color, bracket_start, bracket_alpha);
         bracket_start = bracket_alpha;
      }
      bracket = 2;
   }

   assert(beat_height < 256);
   for (j=0; j < beat_height; ++j)
      alpha_for_height[j] = 1.0;
   for (j=1; j < beat_duration.numerator; ++j) {
      int y = (int) stb_linear_remap(j, 0, beat_duration.numerator, 0,beat_height);
      alpha_for_height[y] = 0.90f;
   }

   if (!hide_measure) {
      for (j=0; j < beat_height; ++j)
         plot_pixel_faded(cursor_x + xpos, cursor_y+j, c, 0.66f, border_line_color);
   }
   xpos += 1;

   // draw the full beats
   for (k=0; k < full_b; ++k) {
      if (!hide_measure) {
         for (j=0; j < beat_height; ++j)
            for (i=0; i < BEAT_WIDTH-1; ++i)
               plot_pixel_faded(cursor_x+i + xpos, cursor_y+j, c, alpha_for_height[j], internal_line_color);
         for (j=0; j < beat_height; ++j)
            plot_pixel_faded(cursor_x+i + xpos, cursor_y+j, c, 0.80f, internal_line_color);
      }
      xpos += BEAT_WIDTH;
   }

   if (frac_b) {
      if (!hide_measure) {
         for (j=0; j < beat_height * frac_b; ++j)
            for (i=0; i < BEAT_WIDTH-1; ++i)
               plot_pixel_faded(cursor_x+i + xpos, cursor_y+j, c, alpha_for_height[j], internal_line_color);
      }
      xpos += BEAT_WIDTH;
   }

   prev_beat_height = beat_height;
   max_beat_height = stb_max(max_beat_height, beat_height);
   delay = 0;

   return xpos;
}

void add_measure(fraction m)
{
   int width = draw_measure(cursor_x, cursor_y, m);
   prev_measure_left = cursor_x;
   prev_measure_right = cursor_x + width;
   cursor_x += width + MEASURE_HORIZONTAL_SPACING;
}

int color_letter;
void set_section_color(int letter)
{
   int i;
   color_letter = letter;
   section_color = -1;
   ghost = 0;
   for (i=0; i < sizeof(color_table)/sizeof(color_table[0]); ++i)
      if ((int) color_table[i][0] == letter)
         section_color = i;
   if (section_color < 0)
      stb_fatal("Error, unknown color '%c' (%d)", letter, letter);
   color_variant = 1;
}

int y_bottom;
void section(char *name, int color)
{
   if (cursor_x != left_edge)
      endline();
   endline();
                               
   #define MIN_TEXT_VERTICAL_SPACING 6
   if (cursor_y < y_bottom)
      cursor_y = y_bottom;

   //draw_text(name,16.0, left_edge-100,cursor_y, 0,0,0, 0);
   if (name[0])
      y_bottom = draw_text(name,16.0, left_edge-25,cursor_y-2, 0,0,0, 1);
      
   set_section_color(color);
}

int hide=1;
unsigned char shown[256];
void section1(char *name, int color)
{
   if (hide && shown[color])
      name = "";
   section(name, color);
   shown[color] = 1;
}


// @TODO pickups

int fade_start_x;
int fade_start_y;

void start_fade_out(void)
{
   fade_start_x = cursor_x;
   fade_start_y = cursor_y;
}

void process_fade_out(void)
{
   int i,j;
   // @TODO handle multiple lines

   for (i=fade_start_x; i < cursor_x; ++i) {
      float a = (float) stb_linear_remap(i, fade_start_x, cursor_x, 0.0, 1.0);
      for (j=fade_start_y; j < cursor_y + max_beat_height; ++j) {
         uint8 *c = &pixels[j*out_w*3 + i*3];
         c[0] = (uint8) stb_lerp(a, c[0], 255);
         c[1] = (uint8) stb_lerp(a, c[1], 255);
         c[2] = (uint8) stb_lerp(a, c[2], 255);
      }
   }
}

char *parse_meter(char *ptr, int default_denominator, fraction *output)
{
   output->numerator = strtol(ptr, &ptr, 10);
   if (*ptr == '/')
      output->denominator = strtol(ptr+1, &ptr, 10);
   else
      output->denominator = default_denominator;
   return ptr;
}

void init_font(void)
{
   int len;
   uint8 *data = stb_fileu("c:/windows/fonts/times.ttf", &len);
   if (data == NULL)
      stb_fatal("Can't find font file (bad Sean for hard-coding it");
   stbtt_InitFont(&font, data, stbtt_GetFontOffsetForIndex(data,0));
}

int bracket;
void set_bracket(int br)
{
   bracket_start = 0;
   if (br == 0) {
      if (bracket == 0) {
         // @TODO: if we try to turn off the bracket when it's already off
         //       ']' without a '[' puts
         //    a special mode that draws a dangling end brack on the *next*
         //    measure, to allow pickups/incomplete measures to show implicit
         //    grouping.
      } else if (bracket == 1) {
         // empty grouping, do nothing
      } else if (bracket == 2) {
         //plot_vline(prev_measure_right+1, cursor_y-2, cursor_y+prev_beat_height+2, bracket_line_color, bracket_alpha);
         //plot_vline(prev_measure_right-1, cursor_y-1, cursor_y, bracket_line_color, bracket_alpha);
         //plot_vline(prev_measure_right-1, cursor_y+prev_beat_height, cursor_y+prev_beat_height+1, bracket_line_color, bracket_alpha);
         plot_hline(prev_measure_left, prev_measure_right, cursor_y-spacing, bracket_line_color, bracket_alpha, 0);
         plot_hline(prev_measure_left, prev_measure_right, cursor_y+prev_beat_height-1+spacing, bracket_line_color, bracket_alpha, 0);
         //cursor_x += 2;
      }
   }
   bracket = br;
}

int line_number;
int main(int argc, char **argv)
{
   int i;
   if (argc <= 1) {
      printf("Usage: songstructure <textfile>+\n"
             "Creates image representing song structure, output as PNG with same name.");
      return 0;
   }

   init_font();

   for (i=1; i < argc; ++i) {
      char *p, *base, *url = NULL, *tweet=0, *tumblr=0, *title=0, *artist=0;
      char filename[1024];
      stb_splitpath(filename, argv[i], STB_PATH_FILE);

      pixels = NULL;
      out_w = out_h = 0;
      max_x = max_y = 0;
      memset(shown, 0, sizeof(shown));
      fade_start_x = fade_start_y = 0;

      beat_duration.numerator = 2;
      beat_duration.denominator = 8;
      default_denominator = 4;
      left_edge = 120;
      cursor_x = left_edge;
      cursor_y = 20;
      section_color = 'v' - 'a' + 1;
      ghost = 0;
      y_bottom = 0;
      max_beat_height = 0;
      hide = 1;
      beat_width=8;
      beat_vscale=1.0;
      bracket = 0;

      base = stb_file(argv[i], NULL);
      if (!base) {
         fprintf(stderr, "Error: couldn't open file '%s'\n", argv[i]);
         exit(1);
      }
      line_number = 1;

      p = base;
      while (*p) {
         switch (*p++) {
            case ';':
               // skip to end of line
               while (*p && *p != '\n' && *p != '\r')
                  ++p;
               break;
            case '\n': case '\r':
               if (p[-1] + p[0] == '\n' + '\r')
                  ++p;
               ++line_number;
               break;
            case 'f': start_fade_out();           break;
            case 'v': section1("Verse"     , 'v'); break;
            case 'c': section1("Chorus"    , 'c'); break;
            case 'b': section1("Bridge"    , 'b'); break;
            case 'i': section1("Intro"     , 'i'); break;
            case 'o': section1("Outro"     , 'o'); break;
            case 'p': section1("Pre-chorus", 'p'); break;
            case 'z': set_section_color(*p); if (*p) ++p; break;
            case 'd': default_denominator = strtol(p, &p, 10); break;
            case '<': pickup = 1; break;
            case '>': delay += 1; break;
            case '(': hide_measure = 1; break;
            case ')': hide_measure = 0; break;
            case '[': set_bracket(1); break;
            case ']': set_bracket(0); break;
            case '`': color_variant = 1; ghost=0; break;
            case '!': color_variant = 2; ghost=0; break;
            case '@': color_variant = 3; ghost=0; break;
            case '#': color_variant = 4; ghost=0; break;
            case '$': color_variant = 5; ghost=0; break;
            case '%': color_variant = 6; ghost=0; break;
            case '^': color_variant = 7; ghost=0; break;
            case '&': color_variant = 8; ghost=0; break;
            case '"': {
                         char buffer[1024];
                         int i;
                         for (i=0; i < sizeof(buffer); ++i) {
                            if (*p == 0 || *p == '"')
                               break;
                            buffer[i] = *p++;
                         }
                         if (*p == '"') ++p;
                         if (i == sizeof(buffer))
                            stb_fatal("Unterminated \" in %s", argv[i]);
                         buffer[i++] = 0;
                         section(buffer, 'v');
                         break;
                      }
            case 's': p = parse_meter(p, 4, &beat_duration);
                      if (beat_duration.numerator == 0 || beat_duration.denominator == 0)
                         stb_fatal("Error: bad measure length in file '%s' line %d\n", argv[i], line_number);
                      break;
            case '-': endline();
                      break;
            case '|': if (cursor_x != left_edge)
                         endline();
                      break;
            case 'g': ghost = 0.75;
                      break;
            #if 0
            case 'g': measure_grouping = strtol(p, &p, 10);
                      if (measure_grouping == 0)
                         stb_fatal("Bad grouping after 'g' in '%s' line %d\n", argv[i], line_number);
                      break;
            #endif

            case 'h': if (0==strnicmp(p, "scale",5)) {
                         float scale = (float) strtod(p+5, &p);
                         beat_width = (int) (scale * 8 + 0.5);
                         beat_width = stb_max(beat_width,2);
                         break;
                      }
                      if (0==strnicmp(p, "eight",5)) {
                         beat_vscale = (float) strtod(p+5, &p);
                         break;
                      }
                      stb_fatal("Unknown command beginning with 'h' in file '%s' line %d", argv[i], line_number);

            case 'u': if (0==strnicmp(p, "rl ", 3)) {
                         p += 3;
                         url = text_line(&p);
                      }
                      break;

            case 'T':
            case 't': if (0==strnicmp(p, "itle", 4)) {
                         p += 4;
                         title = text_line(&p);
                         draw_text_line(title, 30.0, 0,0,0);
                         break;
                      }
                      if (0==strnicmp(p, "witter", 6)) {
                         p += 6;
                         tweet = text_line(&p);
                         break;
                      }
                      if (0==strnicmp(p, "umblr", 5)) {
                         p += 6;
                         tumblr = text_line(&p);
                         break;
                      }
                      stb_fatal("Unknown command beginning with 't' in file '%s' line %d", argv[i], line_number);
            case 'A':
            case 'a': if (0==strnicmp(p, "rtist", 5)) {
                         p += 5;
                         artist = text_line(&p);
                         draw_text_line(artist, 24.0, 0,0,0);
                         break;
                      }
                      stb_fatal("Unknown command beginning with 'a' in file '%s' line %d", argv[i], line_number);
            case 'N':
            case 'n': if (0==strnicmp(p, "ote", 3)) {
                         p = draw_text_line(p+4, 18.0, 0,0,0);
                         break;
                      }
                      if (0 == strnicmp(p, "ohide", 5)) {
                         hide = 0;
                         p += 5;
                         break;
                      }
                      stb_fatal("Unknown command beginning with 'n' in file '%s' line %d", argv[i], line_number);

            default:
               if (isdigit(p[-1])) {
                  fraction f;
                  char *q = p-1;
                  p = parse_meter(p-1, default_denominator, &f);
                  if (f.numerator == 0 || f.denominator == 0)
                     stb_fatal("Error: bad measure length in file '%s'\n", argv[i]);
                  add_measure(f);
               } else if (isspace(p[-1])) {
                  // do nothing
               } else
                  stb_fatal("Error: unknown character '%c'(%d) in file '%s' line %d", p[-1],p[-1], argv[i], line_number);
               break;
         }
      }

      if (fade_start_y != 0)
         process_fade_out();

      plot_pixel(max_x + left_edge - 50, max_y+100, white);
      stbi_write_png(stb_sprintf("%s.png", filename), max_x, max_y, 3, pixels, 3*out_w);
      free(pixels);

      if (tweet || tumblr || url) {
         FILE *f;
         if (!tumblr)
            tumblr = tweet;
         f = fopen(stb_sprintf("%s.tweet", filename), "w");
         if (tweet) fprintf(f, "%s ", tweet);
         if (url) fprintf(f, "%s", url);
         fclose(f);

         f = fopen(stb_sprintf("%s.tumblr", filename), "w");
         if (tumblr) fprintf(f, "<p>%s</p>", tumblr);
         if (url) fprintf(f, "<p><a href=\"%s\">%s</a> on YouTube</p>", url, title);
         fclose(f);
      }
      free(base);
   }

   return 0;
}
