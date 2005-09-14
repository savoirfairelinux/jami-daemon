#include <cc++/socket.h>
#include "FlexLexer.h"

class yyFlexLexer;
class GUIServer;
class TCPStreamLexer : public ost::TCPStream {
public:
	TCPStreamLexer(ost::TCPSocket &socket, GUIServer* gui) : ost::TCPStream(socket) {
    _lexer = new yyFlexLexer(this, this);
		_gui = gui;
	}
	~TCPStreamLexer() { }
	void parse(void);
	void sip();
	void callsip();
  yyFlexLexer *_lexer;
private:
	GUIServer *_gui;
};


