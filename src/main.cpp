
#include <cstdint>
#include <cstddef>
#include <tuple>
#include <jpeglib.h>
#include <unistd.h>
#include <cstring>
#include <setjmp.h>
#include <vector>

const unsigned char INITIALIZE_PRINTER[] = {0x1B,0x40};
const unsigned char PRINT_AND_FEED_PAPER[]= {0x0A};
const unsigned char SELECT_BIT_IMAGE_MODE[] = {0x1B, 0x2A};
const unsigned char SET_LINE_SPACING[] = {0x1B, 0x33};
const unsigned char GS[] = {0x1d};
const unsigned char CUT_PAPER[] = {0x1d, 'V', 0x41, 0x03};
const unsigned char CUT_[] = {0x1d, 'V', 1};

const unsigned char c24[] = {24};

struct my_error_mgr {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};


METHODDEF(void)
my_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  auto *myerr = (my_error_mgr*) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}

class Printer {

  jpeg_decompress_struct cinfo;
  //struct my_error_mgr jerr;
  FILE * infile;
  JSAMPARRAY buffer;
  int row_stride;

  JSAMPLE * image_buffer;
  int image_height;
  int image_width;

  int fd_print_ = 2;

  std::vector<unsigned char> bitmap;
  size_t scanline = 0;

public:
  explicit Printer()
  {
  } 

  void put_scanline_someplace(const unsigned char *_data, size_t _size) {
    if (bitmap.empty()) {
      return;
    }
    
    unsigned char *p = &bitmap[512 * 3 * (scanline / 24) + (scanline % 24) / 8];
    size_t bit = 7 - scanline % 8;

    for (size_t x = 0; x < image_width; ++x, p += 3) {

      auto r = static_cast<float>(_data[x * 3 + 0]);
      auto g = static_cast<float>(_data[x * 3 + 1]);
      auto b = static_cast<float>(_data[x * 3 + 2]);
      
      auto luminance = static_cast<unsigned>(r * 0.3 + g * 0.59 + b * 0.11);
      unsigned char value = (luminance < 127);

      *p |= (value << bit);
    }

    std::ignore = _data;
    std::ignore = _size;
    printf("data %lu, %lu\n", _size, scanline);
    ++ scanline;
  }
  
  void put_scanline_someplace_single(const unsigned char *_data, size_t _size) {
    if (bitmap.empty()) {
      return;
    }
    
    unsigned char *p = &bitmap[512 * 3 * (scanline / 24) + (scanline % 24) / 8];
    size_t bit = 7 - scanline % 8;

    for (size_t x = 0; x < image_width; ++x, p += 3) {
      auto luminance = static_cast<unsigned>(_data[x]);
      unsigned char value = (luminance < 127);

      *p |= (value << bit);
    }

    ++ scanline;
  }



  bool load_jpeg(const char *_filename) {
    if ((infile = fopen(_filename, "rb")) == NULL) {
      fprintf(stderr, "can't open %s\n", _filename);
      return false;
    }

    my_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;
    if (setjmp(jerr.setjmp_buffer)) {
      jpeg_destroy_decompress(&cinfo);
      fclose(infile);
      return 0;
    }
    
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    image_width = cinfo.output_width;
    image_height = cinfo.output_height;

    printf("size %u,%u\n", image_width, image_height);
    bitmap.resize(512 * 3 * ((image_height / 24) + 1));
    if (image_width != 512 || image_height != 512) {
      printf("expect 512,512 only!\n");
      return false;
    }
    
    row_stride = cinfo.output_width * cinfo.output_components;
    buffer = (*cinfo.mem->alloc_sarray) ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

    while (cinfo.output_scanline < cinfo.output_height) {
      /* jpeg_read_scanlines expects an array of pointers to scanlines.
       * Here the array is only one element long, but you could ask for
       * more than one scanline at a time if that's more convenient.
       */
      jpeg_read_scanlines(&cinfo, buffer, 1);
      /* Assume put_scanline_someplace wants a pointer and sample count. */
      if (row_stride == 512) {
        put_scanline_someplace_single(buffer[0], row_stride);
      } else {
        put_scanline_someplace(buffer[0], row_stride);
      }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);

    return true;
  }

  void print_reset() {
    size_t wr;
    wr = ::write(fd_print_, INITIALIZE_PRINTER, 2);
    std::ignore = wr;
  }

  void print_start() {
    size_t wr;
    wr = ::write(fd_print_, INITIALIZE_PRINTER, 2);
    wr = ::write(fd_print_, SET_LINE_SPACING, 2);
    wr = ::write(fd_print_, c24, 1);

    std::ignore = wr;
  }

  void print_imgline(const unsigned char* _data) {
    size_t wr;

    unsigned char widthLSB = (image_width & 0xFF);
    unsigned char widthMSB = ((image_width >> 8) & 0xFF);

    unsigned char data[5];
    memcpy(data, SELECT_BIT_IMAGE_MODE, 2);

    data[2] = 33;
    data[3] = widthLSB;
    data[4] = widthMSB;

    wr = ::write(fd_print_, data, 5);

    wr = ::write(fd_print_, _data, image_width * 3);

    wr = ::write(fd_print_, PRINT_AND_FEED_PAPER, 1);
    std::ignore = wr;
  }

  void print_image() {
    for (int i = 0; i < image_height / 24; ++i) {
      print_imgline(&bitmap[512 * 3 * i]);
      if (i > 2) {
        //return;
      }
    }
  }
  
  void print_line(const char *_line) {
    std::ignore = ::write(fd_print_, _line, strlen(_line));
  }

  void print_cut() {
    size_t wr;
    wr = ::write(fd_print_, CUT_PAPER, sizeof(CUT_PAPER));
    std::ignore = wr;
  }
};

int main(int _argc, const char **_argv) {

  std::ignore = _argc;
  std::ignore = _argv;

  if (_argc < 2) {
    printf("need arg: <filename jpg 512x512>\n");
    return 1;
  }

  Printer printer;
  if (!printer.load_jpeg(_argv[1])) {
    return 1;
  }
  
  printer.print_reset();
  printer.print_line("begin\n");
  printer.print_start();

  printer.print_image();  

  printer.print_reset();
  printer.print_line("end\n");

  printer.print_cut();

  return 0;
}


