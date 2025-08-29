/*
 * Copyright (c) 2017 Darren Smith
 *
 * wampcc is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "wampcc/wampcc.h"

#include <assert.h>
#include <iostream>
#include <memory>

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

    websocket_protocol::options ws_opts;
    ws_opts.serialisers = static_cast<int>(serialiser_type::msgpack);

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
      }, ws_opts);

    /* Logon to a WAMP realm, and wait for session to be deemed open. */

    client_credentials credentials;
    credentials.realm="realm1";
    credentials.authid="john";
    credentials.authmethods = {"cryptosign"};
    credentials.public_key = "4bf3d505110f39d3268147a38776468c4291e2ad690ddcf86c7a3351f2a00c6f";
    credentials.secret_fn = []() -> std::string { return "bf0ad88b15d75447e4f863e80a9d1033097ee368d3013ff9c1eb4a4deca3d096"; };

    auto logon_fut = session->hello(credentials);

    if (logon_fut.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
      throw std::runtime_error("time-out during session logon");

    if(!session->is_open())
      throw std::runtime_error("session logon failed");

    /* Session is now open, call a remote procedure. */
    const char raw_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    size_t raw_size = sizeof(raw_data);
    json_value jbin = json_value::make_binary(raw_data, raw_size);

    wamp_args call_args;
    call_args.args_list = json_array({jbin});
    session->call(rpc_uri, {}, call_args,
                  [=, &ready_to_exit](wampcc::wamp_session&, result_info r) {
                    try {
                      json_value valRet = r.args.args_list[0];
                      json_binary bin = valRet.as_binary();
                      assert(jbin == valRet);
                      std::cout << "rpc result: " << bin.data() << std::endl;
                      ready_to_exit.set_value();
                    } catch (...) {
                      std::cerr << "exception: " << std::endl;
                    }
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
