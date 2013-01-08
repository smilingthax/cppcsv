#include "csvparser.h"
#include <assert.h>
#include <string.h>
#include "csvbase.h"

#include <boost/variant.hpp>

//#define DEBUG

#ifdef DEBUG
  #include <stdio.h>
  #include <typeinfo>
#endif

namespace csvFSM {

// States
struct Start {};
struct ReadSkipPre {};
struct ReadQuoted {};
struct ReadQuotedCheckEscape {};
struct ReadQuotedSkipPost {};
struct ReadUnquoted {};
struct ReadUnquotedWhitespace { 
  ReadUnquotedWhitespace(unsigned char value) : value(1,value) {}
  std::string value;
};
struct ReadError {
  ReadError(const char *type) : type(type) {} // i.e. for static strings
  const char *type;
};
typedef boost::variant<Start,
                       ReadSkipPre,
                       ReadQuoted,
                       ReadQuotedCheckEscape,
                       ReadQuotedSkipPost,
                       ReadUnquoted,
                       ReadUnquotedWhitespace,
                       ReadError> States;

// Events
struct Echar { // and fallback
  Echar(unsigned char value) : value(value) {}
  unsigned char value;
};
struct Ewhitespace : Echar { 
  Ewhitespace(unsigned char value) : Echar(value) {}
};
struct Eqchar : Echar {
  Eqchar(unsigned char value) : Echar(value) {}
};
struct Esep : Echar {
  Esep(unsigned char value) : Echar(value) {}
};
struct Enewline : Echar {
  Enewline(unsigned char value) : Echar(value) {}
};

#define TTS(S,E,Snew,code) bool on(States &self,S &s,const E &e) { code; self=Snew; return true; }
struct Trans {
  Trans(csv_builder &out) : out(out) {}

  /*  Start  eps  { begin_row }  ReadSkipPre */
  TTS(Start, Eqchar,      ReadQuoted(),   { out.begin_row(); });
  TTS(Start, Esep,        ReadSkipPre(),  { out.begin_row(); next_cell(false); });
  TTS(Start, Enewline,    self,           { out.begin_row(); out.end_row(); });
  TTS(Start, Ewhitespace, ReadSkipPre(),  { out.begin_row(); });
  TTS(Start, Echar,       ReadUnquoted(), { out.begin_row(); add(e); });

  /*
  template <typename State>
  TTS(State, Enewline,    Start(), { ("Esep")(); out.end_row(); });  // on(self[_copy],s,Esep(e.value));
  */

  TTS(ReadSkipPre, Eqchar,      ReadQuoted(),   {});
  TTS(ReadSkipPre, Esep,        self,           { next_cell(false); });
  TTS(ReadSkipPre, Enewline,    Start(),        { next_cell(false); out.end_row(); });
  TTS(ReadSkipPre, Ewhitespace, self,           {});
  TTS(ReadSkipPre, Echar,       ReadUnquoted(), { add(e); });

  TTS(ReadQuoted, Eqchar,      ReadQuotedCheckEscape(), {});
  TTS(ReadQuoted, Esep,        self, { add(e); });
//  TTS(ReadQuoted, Enewline,    self, { add(e); });
//  TTS(ReadQuoted, Ewhitespace, self, { add(e); });
  TTS(ReadQuoted, Echar,       self, { add(e); });

  TTS(ReadQuotedCheckEscape, Eqchar,      ReadQuoted(),  { add(e); });
  TTS(ReadQuotedCheckEscape, Esep,        ReadSkipPre(), { next_cell(); });
  TTS(ReadQuotedCheckEscape, Enewline,    Start(),       { next_cell(); out.end_row(); });
  TTS(ReadQuotedCheckEscape, Ewhitespace, ReadQuotedSkipPost(), {});
  TTS(ReadQuotedCheckEscape, Echar,       ReadError("char after possible endquote"), { return false; });

//  TTS(ReadQuotedSkipPost, Eqchar, ReadError("quote after endquote"),        { return false; });
  TTS(ReadQuotedSkipPost, Esep,        ReadSkipPre(), { next_cell(); });
  TTS(ReadQuotedSkipPost, Enewline,    Start(),       { next_cell(); out.end_row(); });
  TTS(ReadQuotedSkipPost, Ewhitespace, self,          {});
  TTS(ReadQuotedSkipPost, Echar,       ReadError("char after endquote"),   { return false; });

  TTS(ReadUnquoted, Eqchar,      ReadError("unexpected quote in unquoted string"), { return false; });
  TTS(ReadUnquoted, Esep,        ReadSkipPre(), { next_cell(); });
  TTS(ReadUnquoted, Enewline,    Start(),       { next_cell(); out.end_row(); });
  TTS(ReadUnquoted, Ewhitespace, ReadUnquotedWhitespace(e.value), {});
  TTS(ReadUnquoted, Echar,       self,          { add(e); });

  TTS(ReadUnquotedWhitespace, Eqchar,      ReadError("unexpected quote after unquoted string"), { return false; });
  TTS(ReadUnquotedWhitespace, Esep,        ReadSkipPre(),  { cell.append(s.value); next_cell(); });
  TTS(ReadUnquotedWhitespace, Enewline,    Start(),        { cell.append(s.value); next_cell(); out.end_row(); });
  TTS(ReadUnquotedWhitespace, Ewhitespace, self,           { s.value.push_back(e.value); });
  TTS(ReadUnquotedWhitespace, Echar,       ReadUnquoted(), { cell.append(s.value); add(e); });

  TTS(ReadError, Echar, self, { return false; });

private:
  inline void add(const Echar &e) { cell.push_back(e.value); }
  inline void next_cell(bool has_content=true) {
    if (has_content) {
      out.cell(cell.c_str(),cell.size());
    } else {
      assert(cell.empty());
      out.cell(NULL,0);
    }
    cell.clear();
  }
private:
  csv_builder &out;
  std::string cell;
};

namespace detail {
  template <class StateVariant,class Event,class Transitions>
  struct NextState : boost::static_visitor<bool> {
    NextState(StateVariant &v,const Event &e,Transitions &t) : v(v),e(e),t(t) {}

    template <class State>
    bool operator()(State &s) const {
#ifdef DEBUG
  printf("%s %c\n",typeid(State).name(),e.value);
#endif
      return t.on(v,s,e);
    }

//    bool operator()(...) const { return false; }
  private:
    StateVariant &v;
    const Event &e;
    Transitions &t;
  };
} // namespace detail

#ifdef CPP11
template <class Transitions=Trans,class StateVariant,class Event>
bool next(StateVariant &state,Event &&event,Transitions &&trans=Transitions()) {
  return boost::apply_visitor(detail::NextState<StateVariant,Event,Transitions>(state,std::forward<Event>(event),std::forward<Transitions>(trans)),state);
}
#else
template <class Transitions,class StateVariant,class Event>
bool next(StateVariant &state,const Event &event,const Transitions &trans=Transitions()) {
  return boost::apply_visitor(detail::NextState<StateVariant,Event,Transitions>(state,event,trans),state);
}
template <class Transitions,class StateVariant,class Event>
bool next(StateVariant &state,const Event &event,Transitions &trans) {
  return boost::apply_visitor(detail::NextState<StateVariant,Event,Transitions>(state,event,trans),state);
}
#endif

} // namespace csvFSM

// TODO?
bool csvparser::operator()(const std::string &line) // {{{
{
  const char *buf=line.c_str();
  return (operator())(buf,line.size());
}
// }}}

bool csvparser::operator()(const char *&buf,int len) // {{{
{
  csvFSM::States state;
  csvFSM::Trans trans(out);
  while (len>0) {
    bool run=true;
    if (*buf==qchar) {
      run=csvFSM::next(state,csvFSM::Eqchar(*buf),trans);
    } else if (*buf==sep) {
      run=csvFSM::next(state,csvFSM::Esep(*buf),trans);
    } else if (*buf==' ') { // TODO? more (but DO NOT collide with sep=='\t')
      run=csvFSM::next(state,csvFSM::Ewhitespace(*buf),trans);
    } else if (*buf=='\n') {
      run=csvFSM::next(state,csvFSM::Enewline(*buf),trans);
    } else {
      run=csvFSM::next(state,csvFSM::Echar(*buf),trans);
    }
    if (!run) {
      csvFSM::ReadError *err=boost::get<csvFSM::ReadError>(&state);
      if (err) {
#ifdef DEBUG
        fprintf(stderr,"csv parse error: %s\n",err->type);
#endif
        errmsg=err->type;
      }
      return true;
    }
    buf++;
    len--;
  }
  return false;
}
// }}}

