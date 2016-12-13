#include "HtmlWrapper.h"

int HtmlWrapper::Init(string header_msg[5])
{
    int rc = printf(
        "Content-Type: text/html\r\n\r\n"
       "<html>"
        "<head>"
        "<meta http-equiv='Content-Type' content='text/html; charset=utf-8' />"
        "<title>Network Programming Homework 3</title>"
        "</head>"
        "<body bgcolor=#336699>"
        "<font face='Courier New' size=2 color=#FFFF99>"
        "<table width='800' border='1'>"
        "<tr>"
        "<td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>"
        "<tr>"
        "<td valign='top' id='m0'></td><td valign='top' id='m1'></td><td valign='top' id='m2'></td><td valign='top' id='m3'></td><td valign='top' id='m4'></td></tr>"
        "</table>"
    ,header_msg[0].c_str(), header_msg[1].c_str(), header_msg[2].c_str(), header_msg[3].c_str(), header_msg[4].c_str());
    fflush(stdout);

    if(rc < 0) {
        slogf(ERROR, "Write message to browser\n");
    }

    return 0;
}

int HtmlWrapper::Final()
{
    int rc = printf(
        "</font>"
        "</body>"
        "</html>");

    return rc;
}

int HtmlWrapper::Print(int index, const char* buff, int size, bool bold)
{
    printf("<script>document.all['m%d'].innerHTML += '%s",index,bold?"<b>":"");

    map<char, string> escape = {
        {'\r', ""},
        {'\n', "<br>"},
        {'\"', "&quot;"},
        {'\'', "&apos;"},
        {'&', "&amp;"},
        {'<', "&lt;"},
        {'>', "&gt;"},
        {' ', "&nbsp;"}
    };
    for(int i = 0; i < size; ++i) {
        auto it = escape.find(buff[i]);
        if(it != escape.end()) {
            printf("%s",escape[buff[i]].c_str());
        } else {
            putchar(buff[i]);
        }
    }
    printf("%s';</script>\n",bold?"<b>":"");
    fflush(stdout);

    return 0;
}
