/*
 * Copyright (c) 2017 Darren Smith
 *
 * wampcc is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "wampcc/wampcc.h"

#include <memory>
#include <iostream>

using namespace wampcc;

int main(int argc, char** argv)
{
  try
  {
    const char* host = "0.0.0.0";
    int port = 8080;
    std::string rpc_uri = "greeting";
    /* Create the wampcc kernel, which provides event and IO threads. */

    std::unique_ptr<kernel> the_kernel(new kernel({}, logger::nolog()));

    /* Create the TCP socket and attempt to connect. */

    std::unique_ptr<tcp_socket> sock(new tcp_socket(the_kernel.get()));
    auto fut = sock->connect(host, port);

    if (fut.wait_for(std::chrono::milliseconds(250)) != std::future_status::ready)
      throw std::runtime_error("timeout during connect");

    if (uverr ec = fut.get())
      throw std::runtime_error("connect failed: " + std::to_string(ec.os_value()) + ", " + ec.message());

    /* Using the connected socket, now create the wamp session object. */

    std::promise<void> ready_to_exit;
    std::shared_ptr<wamp_session> session = wamp_session::create<websocket_protocol>(
      the_kernel.get(),
      std::move(sock),
      [&ready_to_exit](wamp_session&, bool is_open) {
        if (!is_open)
          try {
            ready_to_exit.set_value();
          }
          catch (...) { /* ignore promise already set error */ }
      }, {});

    /* Logon to a WAMP realm, and wait for session to be deemed open. */

    client_credentials credentials;
    credentials.realm="realm1";
    credentials.authid="john";
    credentials.authmethods = {"cryptosign"};
<<<<<<< Updated upstream
    credentials.secret_fn = []() -> std::string { return "10f82a0592fbd228a96a15787198069c3cdc39d8d611d733b17591987e1b6c1e"; };
=======
    credentials.public_key = "d058f7836630303779e026320ec35c509788c597fdaa8f5ae8c28129d81cff01";
    credentials.secret_fn = []() -> std::string { return "48cd7b32543e5c294b354847a20250550e17bf42f41d58d4736e6dbd526d355c"; };
>>>>>>> Stashed changes

    auto logon_fut = session->hello(credentials);

    if (logon_fut.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
      throw std::runtime_error("time-out during session logon");

    if(!session->is_open())
      throw std::runtime_error("session logon failed");

    /* Session is now open, call a remote procedure. */

    wamp_args call_args;
    call_args.args_list = json_array({"hello from basic_caller"});
    session->call(rpc_uri, {}, call_args,
                  [&ready_to_exit](wampcc::wamp_session&, result_info r) {
                    try {
                      std::cout << "rpc result: " << r.args.args_list << std::endl;
                      ready_to_exit.set_value();
<<<<<<< Updated upstream
                    } catch (...) { /* ignore promise already set error */}
=======
                    } catch (...) {
                      std::cout << "fail";
                    }
>>>>>>> Stashed changes
                  });

    /* Wait for RPC completion or until wamp session is closed. */

    ready_to_exit.get_future().wait();
    return 0;
  }
  catch (std::exception& e)
  {
    std::cout << e.what() << std::endl;
    return 1;
  }
}
