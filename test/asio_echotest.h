#pragma once

#define ASIO_STANDALONE
#include "asio.hpp"
#include "asio_echomodels.h"
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
using namespace std::chrono_literals;

namespace asiotest {

using asio::ip::tcp;
void asioechotest(int buffersize = 128)
{
    std::cout << "Starting ASIO Echo Test with buffer size of " << buffersize << " bytes" << std::endl;
    asiomodels::writeechos = 0.0;
    asiomodels::keepgoing = true;
    asiomodels::writeecho.resize(buffersize);
    asiomodels::readecho.resize(buffersize); 

    auto porttouse = static_cast<unsigned short>(std::rand() % 3000 + 10000);
    asio::io_context iocontext;
    asiomodels::asioserver s(iocontext, porttouse);
    s.do_accept();

    tcp::resolver resolver(iocontext);
    auto endpoints = resolver.resolve("127.0.0.1", std::to_string(porttouse));
    asiomodels::asioclient c(iocontext);

    std::thread t([&iocontext]() { iocontext.run(); });
    c.do_connect(endpoints);

    std::this_thread::sleep_for(10s); // sleep for 10 seconds
    asiomodels::keepgoing = false;
    std::cout << "ASIO Echo per Second " << asiomodels::writeechos / 10 << std::endl;
    iocontext.stop();
    s.acceptor_.cancel();
    s.acceptor_.close();
    c.close();
    t.join();
}

} // namespace asiotest
