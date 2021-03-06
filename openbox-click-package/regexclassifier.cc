#include <click/config.h>
#include "regexclassifier.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/args.hh>
#include <click/confparse.hh>
#include <click/router.hh>

CLICK_DECLS

RegexClassifier::RegexClassifier() {
}

RegexClassifier::~RegexClassifier() {
}

int RegexClassifier::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool payload_only = false;
    if (Args(this, errh).bind(conf)
      .read("PAYLOAD_ONLY", payload_only)
      .consume() < 0)
      return -1;
    _payload_only = payload_only;

    if (conf.size() != noutputs())
	   return errh->error("need %d patterns, one per output port", noutputs());
    
    if (!is_valid_patterns(conf, errh)) {
        return -1;
    }

    if (!_program.is_open()) {
        _program.reset();
    }

    for (int i=0; i < conf.size(); ++i) {
        String pattern = cp_unquote(conf[i]);
        int result = _program.add_pattern(pattern); 
        if (result < 0) {
            // This should not happen
            return errh->error("Error (%d) adding pattern %d: %s", result, i, pattern.c_str());
        }
    }

    if (!_program.compile()) {
        // This should not happen
        return errh->error("Unable to compile patterns");
    }


    if (!errh->nerrors()) {
        return 0;
    } else {
        return -1;
    }
}

bool RegexClassifier::is_valid_patterns(Vector<String> &patterns, ErrorHandler *errh) const{
    RegexSet test_set;
    bool valid = true;
    for (int i=0; i < patterns.size(); ++i) {
        String pattern = cp_unquote(patterns[i]);
        int result = test_set.add_pattern(pattern); 
        if (result < 0) {
            errh->error("Error (%d) in pattern %d: %s", result, i, pattern.c_str());
            valid = false;
        }
    }
    if (valid) {
        // Try to compile 
        valid = test_set.compile();
    }

    return valid;
}

enum { H_PAYLOAD_ONLY};

String
RegexClassifier::read_handler(Element *e, void *thunk)
{
    RegexClassifier *c = (RegexClassifier *)e;
    switch ((intptr_t)thunk) {
      case H_PAYLOAD_ONLY:
          return String(c->_payload_only);
      default:
          return "<error>";
    }
}

int
RegexClassifier::write_handler(const String &in_str, Element *e, void *thunk, ErrorHandler *errh)
{
    RegexClassifier *c = (RegexClassifier *)e;
    switch ((intptr_t)thunk) {
        case H_PAYLOAD_ONLY:
           if (!BoolArg().parse(in_str, c->_payload_only))
                return errh->error("syntax error");
           return 0;
        default:
            return errh->error("<internal>");
    }
}

void 
RegexClassifier::add_handlers() {
    for (uintptr_t i = 0; i != (uintptr_t) noutputs(); ++i) {
    	add_read_handler("pattern" + String(i), read_positional_handler, (void*) i);
    	add_write_handler("pattern" + String(i), reconfigure_positional_handler, (void*) i);
    }
    add_read_handler("payload_only", read_handler, H_PAYLOAD_ONLY);
    add_write_handler("payload_only", write_handler, H_PAYLOAD_ONLY);
}

void 
RegexClassifier::push(int, Packet* p) {
    char* data = (char *) p->data();
    int length = p->length();
    if (_payload_only) {
        if (p->has_network_header()) {
            data = (char *) p->network_header();
            length = p->network_length();
        }
        if (p->has_transport_header()) {
            data = (char *) p->transport_header();
            length = p->transport_length();
        }
    }
    checked_output_push(_program.match_first(data, length), p);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RegexClassifier)
ELEMENT_REQUIRES(userlevel RegexSet)
ELEMENT_MT_SAFE(RegexClassifier)
