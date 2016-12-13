#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <error.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "Logger.h"
#include "HtmlWrapper.h"
#include "util.h"
#include "coroutine.h"

#define NO 0
#define WRITE 1
#define READ 4
#define CONNECT 2
#define CLOSE 3

using namespace std;

int lastRegisteredEvent[6] = {};
int reachedEvent[6] = {};

struct args {
    args() { connfd = 0; id = 0; file = NULL; }
	int connfd;
    int id;
    FILE *file;
};

void serve(schedule *S, void *ud) {
    args *arg = (args*)ud;
    int connfd = arg->connfd;
    int id = arg->id;
    FILE* file = arg->file;

    bool eof = false;
    bool hasPrompt = false;
    while(1) {
        hasPrompt = false;
        //Read() start
        lastRegisteredEvent[id] = READ;
        coroutine_yield(S);
        if(reachedEvent[id] == CLOSE)
            return;
        for(;;) {
            if(reachedEvent[id] != READ)
                slogf(ERROR, "reached event not matches %d\n",reachedEvent[id]);
            char buff[1024] = {};
            int len = read(connfd, buff, 1024);
            if(len < 0) {
                if(errno == EAGAIN) {
                    break;
                } else {
                    slogf(ERROR, "read error %s\n",strerror(errno));
                }
            } else if(len == 0) {
                return;
            } else {
                HtmlWrapper::Print(id-1, buff, len, false);
                for(int i = 0; i < len; ++i) {
                    if(buff[i]=='%' && buff[i+1] == ' ')
                        hasPrompt = true;
                }
            }

            if(len != 1024)
                break;
        }
        //Read() end

        // fgets() + Write() start
        for(; !eof && hasPrompt ;) {
            // retrieve one line
            char buff[1024] = {};
            char *rc = fgets(buff, 1023, file);
            if(rc == NULL) { // EOF
                eof = true;
                break;
            }

            // try to send it
            int len = strlen(buff);
            const char *ptr = buff;

            HtmlWrapper::Print(id-1, buff, len, true);

            while(ptr < buff+len) {
                int rc = write(connfd, ptr, len);
                if(rc < 0) {
                    if(errno == EAGAIN) {
                        lastRegisteredEvent[id] = WRITE;
                        coroutine_yield(S);
                        if(reachedEvent[id] == WRITE)
                            continue;
                        else if(reachedEvent[id] == CLOSE)
                            return;
                        else
                            slogf(ERROR, "reached event not matches %d\n",reachedEvent[id]);
                    } else {
                        slogf(ERROR, "write error %s\n",strerror(errno));
                    }
                } 
                ptr+=rc;
            }

            if(len != 1022) // contains new line char
                break;
        }
        // fgets() + Write() end
    }
}

int main()
{
    map<string, string> querys = queryExtract();

    ////////////////////////////////////////
    /*           epoll variables          */
    int epoll_fd = epoll_create(10);
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    ////////////////////////////////////////

    ////////////////////////////////////////
    /*        coroutine variable          */
    struct schedule *S = coroutine_open();
    args info[6];
    ////////////////////////////////////////

    ////////////////////////////////////////
    /*        connection setup            */
    int conn_count = 0;
    string header_msg[5] = {};
    for(int i = 1; i <= 5; ++i) {
        int rc = getConnSocket(querys["h"+to_string(i)], querys["p"+to_string(i)]);
        FILE* file = fopen(querys["f"+to_string(i)].c_str(), "r");
        if(rc < 0) {
            if(querys["h"+to_string(i)] =="" || querys["p"+to_string(i)] == "")
                header_msg[i-1] = "Missing setting";
            else
                header_msg[i-1] = "[FAIL] Connect to " + querys["h"+to_string(i)] + "/" + querys["p"+to_string(i)];

            slogf(WARN, "%d open conn failed\n",i);
            if(file)    fclose(file);
            continue;
        }
        if(!file) {
            header_msg[i-1] = "[FAIL] Open file " + querys["h"+to_string(i)];
            slogf(WARN, "%d open file failed\n",i);
            close(rc);
            continue;
        }

        info[i].connfd = rc;
        info[i].file = file;
        info[i].id = i;
        event.data.fd = i;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, rc, &event);
        conn_count++;

        header_msg[i-1] = querys["h"+to_string(i)] + "/" + querys["p"+to_string(i)];
    }

    HtmlWrapper::Init(header_msg);
    slogf(INFO, "Setup Ok!\n");
    ////////////////////////////////////////

    ////////////////////////////////////////
    /*        coroutine setup             */
    int cr[6] = {};
    for(int i = 1; i <= 5; ++i) {
        if(info[i].connfd != 0) {
            cr[i] = coroutine_new(S, serve, &info[i]);
        }
    } 
    ////////////////////////////////////////
    
    slogf(INFO, "Start main loop\n");

    while (conn_count != 0) {
        struct epoll_event events[10];
        int rc = epoll_wait(epoll_fd, events, 10, -1); // -1 for block until anyone ready
        if (rc < 0) {
            printf("epoll_wait err\n");
            exit(0);
        }
        for (int i = 0; i < rc; ++i) {
            int id = events[i].data.fd;
            /* handle disconnected events */
            if (events[i].events & EPOLLERR || events[i].events & EPOLLHUP || events[i].events & EPOLLRDHUP) {
                conn_count--;
                reachedEvent[id] = CLOSE;

                if(coroutine_status(S, cr[id]) != COROUTINE_DEAD) {
                    coroutine_resume(S, cr[id]);
                } else {
                    slogf(ERROR, "resume a dead coroutine %d\n",id);
                }

                continue;
            }

            /* handle readable*/
            if (events[i].events & EPOLLIN) {
                if(lastRegisteredEvent[id] == READ || lastRegisteredEvent[id] == NO) {
                    //slogf(ERROR, "registered event not matches reached event for cr %d\n",id);
                    reachedEvent[id] = READ;

                    if(coroutine_status(S, cr[id]) != COROUTINE_DEAD) {
                        coroutine_resume(S, cr[id]);
                        if(lastRegisteredEvent[id] == WRITE) {
                            event.events = EPOLLIN | EPOLLOUT | EPOLLET;
                            epoll_ctl(epoll_fd, EPOLL_CTL_MOD, info[id].connfd, &event);
                        }
                        if(coroutine_status(S, cr[id]) == COROUTINE_DEAD)
                            conn_count--;
                    } else {
                        slogf(ERROR, "resume a dead coroutine %d\n",id);
                    }
                }
            }


            /* 觸發情況
             * 1. 連線第一次建立
             * 2. 緩衝區由滿 -> 可寫
             * 3. 重新被加入epoll_fd
             * 總之就是writable
             */
            if (events[i].events & EPOLLOUT) {
                /* connected */
                if(lastRegisteredEvent[id] == NO) {
                    reachedEvent[id] = CONNECT;
                } else {
                    if(lastRegisteredEvent[id] != WRITE) 
                        continue;
                        //slogf(ERROR, "registered event %d not matches reached event EPOLLOUT for cr %d\n",lastRegisteredEvent[id],id);
                    reachedEvent[id] = WRITE;
                }

                if(coroutine_status(S, cr[id]) != COROUTINE_DEAD) {
                    slogf(INFO, "coroutine resume %d [%d]\n",id,reachedEvent[id]);
                    coroutine_resume(S, cr[id]);
                    if(coroutine_status(S, cr[id]) == COROUTINE_DEAD)
                        conn_count--;
                } else {
                    slogf(ERROR, "resume a dead coroutine %d\n",id);
                }
            }

        }
    }

    HtmlWrapper::Final();
    coroutine_close(S);

    return 0;
}

