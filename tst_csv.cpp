#include <stdio.h>
#include <assert.h>
#include "csvparser.h"
#include "csvwriter.h"
#include "simplecsv.h"

#if !defined(__GXX_EXPERIMENTAL_CXX0X__)&&(__cplusplus<201103L)
  #define override
#endif

class debug_builder : public csv_builder {
public:
  void begin_row() override { 
    printf("begin_row\n");
  }
  void cell(const char *buf,int len) override {
    if (!buf) {
      printf("(null) ");
    } else {
      printf("\"%.*s\" ",len,buf);
    }
  }
  void end_row() override {
    printf("end_row\n");
  }
};

class null_builder : public csv_builder {
public:
  void cell(const char *buf,int len) override {}
};

struct file_out {
  file_out(FILE *f) : f(f) { assert(f); }

  void operator()(const char *buf,int len) {
    fwrite(buf,1,len,f);
  }
private:
  FILE *f;
};

int main(int argc,char **argv)
{
//  debug_builder dbg;
//  null_builder dbg;
//  csv_writer<file_out> dbg((file_out(stdout))); // CPP11: dbg{{stdout}}
//  csv_writer<file_out> dbg(file_out(stdout),'\'',',',true);

#if 1
  SimpleCSV::Table tbl;
  SimpleCSV::builder dbg(tbl);
#endif

  csvparser cp(dbg,'\'');
  cp(
    "\n"
    "1, 's' , 3,4   a\n"
    ",1,2,3,4\n"
    " asdf, 'asd''df', s\n"
  );

  csv_writer<file_out> dbg2(file_out(stdout),'\'',',',true);
  tbl.write(dbg2);

  return 0;
}

