#ifndef __TCPSTREAMLEXER_H__
#define __TCPSTREAMLEXER_H__
#include <cc++/socket.h>
//#include "y.tab.h"
#include "FlexLexer.h"
static FlexLexer* lexer;

class GUIServer;
class TCPStreamLexer : public ost::TCPStream {
public:
	TCPStreamLexer(ost::TCPSocket &socket, GUIServer* gui) : ost::TCPStream(socket) {
		_gui = gui;
		lexer = new yyFlexLexer(this, this);
	}
	~TCPStreamLexer() { delete lexer; }
	void parse(void);
	void sip(YYSTYPE &y);
	void callsip(YYSTYPE &y);
private:
	GUIServer *_gui;
};

#endif // __TCPSTREAMLEXER_H__

