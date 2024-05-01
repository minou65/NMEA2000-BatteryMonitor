// IotWebRoot.h

#ifndef _IOTWEBROOT_h
#define _IOTWEBROOT_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

// -- HTML page fragments
const char IOTWEBROOT_HTML_HEAD[] PROGMEM = "<!DOCTYPE html>\n<html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>\n<title>{v}</title>\n";
const char IOTWEBROOT_HTML_HEAD_END[] PROGMEM = "</head>\n<body>\n";
const char IOTWEBROOT_HTML_END[] PROGMEM = "</body>\n</html>\n";
const char IOTWEBROOT_HTML_STYLE_INNER[] PROGMEM = ".de{background-color:#ffaaaa;} .em{font-size:0.8em;color:#bb0000;padding-bottom:0px;} .c{text-align: center;} div,input,select{padding:5px;font-size:1em;} input{width:95%;} select{width:100%} input[type=checkbox]{width:auto;scale:1.5;margin:10px;} body{text-align: center;font-family:verdana;} button{border:0;border-radius:0.3rem;background-color:#16A1E7;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;} fieldset{border-radius:0.3rem;margin: 0px;}\n";
const char IOTWEBROOT_HTML_BODY_INNER[] PROGMEM = "<div style='text-align:left;display:inline-block;min-width:260px;'>\n";
const char IOTWEBROOT_HTML_FIELDSET[] PROGMEM = "<fieldset align=left style=\"border: 1px solid\"><legend>{v}</legend>\n";
const char IOTWEBROOT_HTML_FIELDSET_END[] PROGMEM = "</fieldset>\n";
const char IOTWEBROOT_HTML_TABLE[] PROGMEM = "<table border=\"0\" align=\"center\">\n";
const char IOTWEBROOT_HTML_TABLE_END[] PROGMEM = "</table>\n";
const char IOTWEBROOT_HTML_TABLE_ROW[] PROGMEM = "<tr>\n";
const char IOTWEBROOT_HTML_TABLE_ROW_END[] PROGMEM = "</tr>\n";
const char IOTWEBROOT_HTML_TABLE_COL[] PROGMEM = "<td align=\"left\">\n";
const char IOTWEBROOT_HTML_TABLE_COL_END[] PROGMEM = "</td>\n";
const char IOTWEBROOT_HTML_TABLE_ROW_TEXT[] PROGMEM = "<tr><td align=\"left\">{n}</td><td align=\"left\">{v}</td></tr>\n";
const char IOTWEBROOT_HTML_TABLE_ROW_SPAN[] PROGMEM = "<tr><td align=\"left\">{n}</td><td align=\"left\"><span id=\"{id}\">{v}</span></td></tr>\n";
const char IOTWEBROOT_HTML_SCRIPT_INNNER[] PROGMEM = "\n";
const char IOTWEBROOT_HTML_CONFIG_VER[] PROGMEM = "<div style='font-size: .6em;'>Version '{v}'</div>\n";
const char IOTWEBROOT_HTML_SCRIPT_INNER_JS_REQUESTDATA[] PROGMEM = "requestData()\nsetInterval(requestData, {millisecond});\nfunction requestData() {\n    var xhttp = new XMLHttpRequest();\n    xhttp.onreadystatechange = function() {\n        if (this.readyState == 4 && this.status == 200) {\n            var json = JSON.parse(this.responseText);\n            updateData(json);\n        }\n    };\n    xhttp.open('GET', 'data', true);\n    xhttp.send();\n}\n";

class HtmlRootFormatProvider {
public:
	virtual String getHtmlHead(String title) {
		String s = FPSTR(IOTWEBROOT_HTML_HEAD);
		s.replace("{v}", title);
		return s;
	}

	virtual String getHtmlHeadEnd() {
		String s = FPSTR(IOTWEBROOT_HTML_HEAD_END);
		s += "<style>\n" + getBodyInner() + "</style>\n";
		return s;
	}

	virtual String getHtmlEnd() {
		return FPSTR(IOTWEBROOT_HTML_END);
	}

	virtual String getHtmlStyle() {
		return "<style>\n" + getStyleInner() + "</style>\n";
	}

	virtual String getHtmlFieldset(String title) {
		String s = FPSTR(IOTWEBROOT_HTML_FIELDSET);
		s.replace("{v}", title);
		return s;
	}

	virtual String getHtmlFieldsetEnd() {
		return FPSTR(IOTWEBROOT_HTML_FIELDSET_END);
	}

	virtual String getHtmlTable() {
		return FPSTR(IOTWEBROOT_HTML_TABLE);
	}

	virtual String getHtmlTableEnd() {
		return FPSTR(IOTWEBROOT_HTML_TABLE_END);
	}

	virtual String getHtmlTableRowText(String name, String value = "") {
		String s = FPSTR(IOTWEBROOT_HTML_TABLE_ROW_TEXT);
		s.replace("{n}", name);
		s.replace("{v}", value);
		return s;
	}

	virtual String getHtmlTableRowSpan(String name, String value, String id) {
		String s = FPSTR(IOTWEBROOT_HTML_TABLE_ROW_SPAN);
		s.replace("{n}", name);
		s.replace("{v}", value);
		s.replace("{id}", id);
		return s;
	}

	virtual String getHtmlScript() {
		return "<script>\n" + getScriptInner() + "\n</script>\n";
	}

	virtual String getHtmlVersion(String version) {
		String s = FPSTR(IOTWEBROOT_HTML_CONFIG_VER);
		s.replace("{v}", version);
		return s;
	}

	virtual String addNewLine(uint8_t count = 1) {
		String s = "";
		for (uint8_t i = 0; i < count; i++) {
			s += "<br>\n";
		}
		return s;
	}

	virtual String getHtmlTableRow() {
		return FPSTR(IOTWEBROOT_HTML_TABLE_ROW);
	}

	virtual String getHtmlTableCol() {
		return FPSTR(IOTWEBROOT_HTML_TABLE_COL);
	}

	virtual String getHtmlTableColEnd() {
		return FPSTR(IOTWEBROOT_HTML_TABLE_COL_END);
	}

	virtual String getHtmlTableRowEnd() {
		return FPSTR(IOTWEBROOT_HTML_TABLE_ROW_END);
	}

protected:
	virtual String getStyleInner() { return FPSTR(IOTWEBROOT_HTML_STYLE_INNER); }
	virtual String getScriptInner() { return FPSTR(IOTWEBROOT_HTML_SCRIPT_INNER_JS_REQUESTDATA); }
	virtual String getBodyInner() { return FPSTR(IOTWEBROOT_HTML_BODY_INNER); }
};

#endif

