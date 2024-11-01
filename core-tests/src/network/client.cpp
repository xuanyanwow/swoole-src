#include "test_core.h"
#include "test_server.h"
#include "test_process.h"

#define GREETER "Hello Swoole"
#define GREETER_SIZE sizeof(GREETER)

using swoole::HttpProxy;
using swoole::Pipe;
using swoole::Socks5Proxy;
using swoole::network::AsyncClient;
using swoole::network::Client;
using swoole::test::Process;
using swoole::test::Server;

TEST(client, tcp) {
    int ret;
    char buf[128];

    pid_t pid;

    Process proc([](Process *proc) {
        on_receive_lambda_type receive_fn = [](ON_RECEIVE_PARAMS) {
            SERVER_THIS->send(req->info.fd, req->data, req->info.len);
        };

        Server serv(TEST_HOST, TEST_PORT, swoole::Server::MODE_BASE, SW_SOCK_TCP);
        serv.on("onReceive", (void *) receive_fn);
        serv.start();
    });

    pid = proc.start();

    sleep(1);  // wait for the test server to start

    Client cli(SW_SOCK_TCP, false);
    ASSERT_NE(cli.socket, nullptr);
    ret = cli.connect(&cli, TEST_HOST, TEST_PORT, -1, 0);
    ASSERT_EQ(ret, 0);
    ret = cli.send(&cli, SW_STRS(GREETER), 0);
    ASSERT_GT(ret, 0);
    ret = cli.recv(&cli, buf, 128, 0);
    ASSERT_EQ(ret, GREETER_SIZE);
    ASSERT_STREQ(GREETER, buf);

    kill(pid, SIGTERM);
    int status;
    wait(&status);
}

TEST(client, udp) {
    int ret;
    char buf[128];

    pid_t pid;

    Process proc([](Process *proc) {
        on_packet_lambda_type packet_fn = [](ON_PACKET_PARAMS) {
            swoole::DgramPacket *packet = (swoole::DgramPacket *) req->data;
            SERVER_THIS->sendto(packet->socket_addr, packet->data, packet->length, req->info.server_fd);
        };

        Server serv(TEST_HOST, TEST_PORT, swoole::Server::MODE_BASE, SW_SOCK_UDP);
        serv.on("onPacket", (void *) packet_fn);
        serv.start();
    });

    pid = proc.start();

    sleep(1);  // wait for the test server to start

    Client cli(SW_SOCK_UDP, false);
    ASSERT_NE(cli.socket, nullptr);
    ret = cli.connect(&cli, TEST_HOST, TEST_PORT, -1, 0);
    ASSERT_EQ(ret, 0);
    ret = cli.send(&cli, SW_STRS(GREETER), 0);
    ASSERT_GT(ret, 0);
    ret = cli.recv(&cli, buf, 128, 0);
    ASSERT_EQ(ret, GREETER_SIZE);
    ASSERT_STREQ(GREETER, buf);

    kill(pid, SIGTERM);
    int status;
    wait(&status);
}

static void test_async_client_tcp(const char *host, int port) {
    pid_t pid;

    Pipe p(true);
    ASSERT_TRUE(p.ready());

    Process proc([&p](Process *proc) {
        on_receive_lambda_type receive_fn = [](ON_RECEIVE_PARAMS) {
            SERVER_THIS->send(req->info.fd, req->data, req->info.len);
        };

        Server serv(TEST_HOST, TEST_PORT, swoole::Server::MODE_BASE, SW_SOCK_TCP);

        serv.set_private_data("pipe", &p);

        serv.on("onReceive", (void *) receive_fn);

        on_workerstart_lambda_type worker_start_fn = [](ON_WORKERSTART_PARAMS) {
            Pipe *p = (Pipe *) SERVER_THIS->get_private_data("pipe");
            int64_t value = 1;
            p->write(&value, sizeof(value));
        };

        serv.on("onWorkerStart", (void *) worker_start_fn);

        serv.start();
    });

    pid = proc.start();
    int64_t value;
    p.set_timeout(10);
    p.read(&value, sizeof(value));

    swoole_event_init(SW_EVENTLOOP_WAIT_EXIT);

    AsyncClient ac(SW_SOCK_TCP);

    ac.on_connect([](AsyncClient *ac) { ac->send(SW_STRS(GREETER)); });

    ac.on_close([](AsyncClient *ac) {

    });
    ac.on_error([](AsyncClient *ac) {

    });

    ac.on_receive([](AsyncClient *ac, const char *data, size_t len) {
        ASSERT_EQ(len, GREETER_SIZE);
        ASSERT_STREQ(GREETER, data);
        ac->close();
    });

    bool retval = ac.connect(host, port);
    EXPECT_TRUE(retval);

    swoole_event_wait();

    kill(pid, SIGTERM);
    int status;
    wait(&status);
}

TEST(client, async_tcp) {
    test_async_client_tcp(TEST_HOST, TEST_PORT);
}

TEST(client, async_tcp_dns) {
    test_async_client_tcp("localhost", TEST_PORT);
}

TEST(client, connect_refuse) {
    int ret;
    Client cli(SW_SOCK_TCP, false);
    ret = cli.connect(&cli, TEST_HOST, TEST_PORT + 10001, -1, 0);
    ASSERT_EQ(ret, -1);
    ASSERT_EQ(swoole_get_last_error(), ECONNREFUSED);
}

TEST(client, connect_timeout) {
    int ret;
    Client cli(SW_SOCK_TCP, false);
    ret = cli.connect(&cli, "19.168.0.99", TEST_PORT + 10001, 0.2, 0);
    ASSERT_EQ(ret, -1);
    ASSERT_EQ(swoole_get_last_error(), ETIMEDOUT);
}

TEST(client, shutdown_write) {
    signal(SIGPIPE, SIG_IGN);
    int ret;
    Client cli(SW_SOCK_TCP, false);
    ret = cli.connect(&cli, "www.baidu.com", 80, -1, 0);
    ASSERT_EQ(ret, 0);
    cli.shutdown(SHUT_WR);
    ssize_t retval = cli.send(&cli, SW_STRL("hello world"), 0);
    ASSERT_EQ(retval, -1);
    ASSERT_EQ(swoole_get_last_error(), EPIPE);
}

TEST(client, shutdown_read) {
    signal(SIGPIPE, SIG_IGN);
    int ret;
    Client cli(SW_SOCK_TCP, false);
    ret = cli.connect(&cli, "www.baidu.com", 80, -1, 0);
    ASSERT_EQ(ret, 0);

    cli.shutdown(SHUT_RD);
    ssize_t retval = cli.send(&cli, SW_STRL("hello world\r\n\r\n"), 0);
    ASSERT_GT(retval, 0);

    char buf[1024];
    retval = cli.recv(&cli, buf, sizeof(buf), 0);
    ASSERT_EQ(retval, 0);
}

TEST(client, shutdown_all) {
    signal(SIGPIPE, SIG_IGN);
    int ret;
    Client cli(SW_SOCK_TCP, false);
    ret = cli.connect(&cli, "www.baidu.com", 80, -1, 0);
    ASSERT_EQ(ret, 0);

    cli.shutdown(SHUT_RDWR);

    ssize_t retval = cli.send(&cli, SW_STRL("hello world\r\n\r\n"), 0);
    ASSERT_EQ(retval, -1);
    ASSERT_EQ(swoole_get_last_error(), EPIPE);

    char buf[1024];
    retval = cli.recv(&cli, buf, sizeof(buf), 0);
    ASSERT_EQ(retval, 0);
}

#ifdef SW_USE_OPENSSL

static const char *request_baidu = "GET / HTTP/1.1\r\n"
        "Host: www.baidu.com\r\n"
        "Connection: close\r\n"
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/51.0.2704.106 Safari/537.36"
        "\r\n\r\n";

static const char *domain_baidu = "www.baidu.com";

#define SOCKS5_WITH_AUTH  1

TEST(client, ssl_1) {
    bool connected = false;
    bool closed = false;
    swoole::String buf(65536);

    swoole_event_init(SW_EVENTLOOP_WAIT_EXIT);

    Client client(SW_SOCK_TCP, true);
    client.enable_ssl_encrypt();
    client.onConnect = [&connected](Client *cli) {
        connected = true;
        cli->send(cli, request_baidu, strlen(request_baidu), 0);
    };

    client.onError = [](Client *cli) {};
    client.onClose = [&closed](Client *cli) { closed = true; };
    client.onReceive = [&buf](Client *cli, const char *data, size_t length) { buf.append(data, length); };

    ASSERT_EQ(client.connect(&client, domain_baidu, 443, -1, 0), 0);

    swoole_event_wait();

    ASSERT_TRUE(connected);
    ASSERT_TRUE(closed);
    ASSERT_TRUE(buf.contains("Baidu"));
}

static void proxy_async_test(Client &client, bool https) {
    int ret;

    swoole_event_init(SW_EVENTLOOP_WAIT_EXIT);

    bool connected = false;
    bool closed = false;
    swoole::String buf(65536);

    if (https) {
        client.enable_ssl_encrypt();
    }

    client.onConnect = [&connected](Client *cli) {
        connected = true;
        cli->send(cli, request_baidu, strlen(request_baidu), 0);
    };

    client.onError = [](Client *cli) {};
    client.onClose = [&closed](Client *cli) { closed = true; };
    client.onReceive = [&buf](Client *cli, const char *data, size_t length) { buf.append(data, length); };

    ASSERT_EQ(client.connect(&client, domain_baidu, https ? 443 : 80, -1, 0), 0);

    swoole_event_wait();

    ASSERT_TRUE(connected);
    ASSERT_TRUE(closed);
    ASSERT_TRUE(buf.contains("www.baidu.com"));
}

static void proxy_sync_test(Client &client, bool https) {
    swoole::String buf(65536);
    if (https) {
        client.enable_ssl_encrypt();
    }

    ASSERT_EQ(client.connect(&client, domain_baidu, https ? 443 : 80, -1, 0), 0);
    ASSERT_GT(client.send(&client, request_baidu, strlen(request_baidu), 0), 0);

    while(true) {
        char rbuf[4096];
        auto nr = client.recv(&client, rbuf, sizeof(rbuf), 0);
        if (nr <= 0) {
            break;
        }
        buf.append(rbuf, nr);
    }

    ASSERT_TRUE(buf.contains("www.baidu.com"));
}

static void proxy_set_socks5_proxy(Client &client) {
    client.socks5_proxy = new Socks5Proxy();
    client.socks5_proxy->host = std::string("127.0.0.1");
    client.socks5_proxy->port = 1080;
    client.socks5_proxy->dns_tunnel = 1;
#if SOCKS5_WITH_AUTH
    client.socks5_proxy->method = SW_SOCKS5_METHOD_AUTH;
    client.socks5_proxy->username = std::string("user");
    client.socks5_proxy->password = std::string("password");
#endif
}

static void proxy_set_http_proxy(Client &client) {
    client.http_proxy = new HttpProxy();
    client.http_proxy->proxy_host = std::string(TEST_HTTP_PROXY_HOST);
    client.http_proxy->proxy_port = TEST_HTTP_PROXY_PORT;
}

TEST(client, https_get_async_with_http_proxy) {
    Client client(SW_SOCK_TCP, true);
    proxy_set_http_proxy(client);
    proxy_async_test(client, true);
}

TEST(client, https_get_async_with_socks5_proxy) {
    Client client(SW_SOCK_TCP, true);
    proxy_set_socks5_proxy(client);
    proxy_async_test(client, true);
}

TEST(client, https_get_sync_with_http_proxy) {
    Client client(SW_SOCK_TCP, false);
    proxy_set_http_proxy(client);
    proxy_sync_test(client, true);
}

TEST(client, https_get_sync_with_socks5_proxy) {
    Client client(SW_SOCK_TCP, false);
    proxy_set_socks5_proxy(client);
    proxy_sync_test(client, true);
}

TEST(client, http_get_async_with_http_proxy) {
    Client client(SW_SOCK_TCP, true);
    proxy_set_http_proxy(client);
    proxy_async_test(client, false);
}

TEST(client, http_get_async_with_socks5_proxy) {
    Client client(SW_SOCK_TCP, true);
    proxy_set_socks5_proxy(client);
    proxy_async_test(client, false);
}

TEST(client, http_get_sync_with_http_proxy) {
    Client client(SW_SOCK_TCP, false);
    proxy_set_http_proxy(client);
    proxy_sync_test(client, false);
}

TEST(client, http_get_sync_with_socks5_proxy) {
    Client client(SW_SOCK_TCP, false);
    proxy_set_socks5_proxy(client);
    proxy_sync_test(client, false);
}
#endif
