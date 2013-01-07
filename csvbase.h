#ifndef _CSVBASE_H
#define _CSVBASE_H

class csv_builder { // abstract base
public:
  virtual ~csv_builder() {}

  virtual void begin_row() {}
  virtual void cell(const char *buf,int len) =0;  // buf can be NULL
  virtual void end_row() {}
};

#endif
