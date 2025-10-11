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

#define RANDOM_SIZE 700

// Declare global array
unsigned char random_bytes[RANDOM_SIZE];

// Function to fill the array with random data
void generate_random_bytes(void) {
  srand((unsigned int) time(NULL));
  for (int i = 0; i < RANDOM_SIZE; i++) {
    random_bytes[i] = rand() % 256;
  }
}

/* The callback function invoked when a wamp dealer has completed the procedure registration. */
void rpc_registered(std::promise<void>& ready_to_exit, wamp_session& ws, registered_info info)
{
  std::cout << "rpc registration "
            << (info.was_error? "failed, " + info.error_uri : "success")
            << std::endl;
  if (info.was_error)
    ready_to_exit.set_value();
}

/* The callback function invoked when a wamp INVOCATION message is received from a
 * wamp dealer. A result is sent back to the dealer by calling the yield() method. */
void rpc_called(wamp_session& ws, invocation_info invoke)
{
  std::cout << "rpc invoked" << std::endl;
  size_t raw_size = sizeof(random_bytes);
  json_value jbin = json_value::make_binary(random_bytes, raw_size);
  ws.yield(invoke.request_id, json_array({jbin}));
}

int main(int argc, char** argv)
{
  try
  {
    const char* host = "127.0.0.1";
    int port = 8080;
    generate_random_bytes();

    /* Create the wampcc kernel, which provides event and IO threads. */

    std::unique_ptr<kernel> the_kernel(new kernel({}, logger::console()));

    /* Create the TCP socket and attempt to connect. */

    std::unique_ptr<tcp_socket> sock (new tcp_socket(the_kernel.get()));
    auto fut = sock->connect(host, port);

    if (fut.wait_for(std::chrono::milliseconds(250)) != std::future_status::ready)
      throw std::runtime_error("timeout during connect");

    if (uverr ec = fut.get())
      throw std::runtime_error("connect failed: " + std::to_string(ec.os_value()) + ", " + ec.message());

    /* Using the connected socket, now create the wamp session object, using
       the WebSocket protocol. */

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

    auto logon_fut = session->hello(credentials);

    if (logon_fut.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
      throw std::runtime_error("time-out during session logon");

    if(!session->is_open())
      throw std::runtime_error("session logon failed");

    /* Session is now open, register an RPC. */

    session->provide("greeting", json_object(),
                     [&ready_to_exit](wamp_session& ws, registered_info info){
                       rpc_registered(ready_to_exit, ws, info);
                     },
                     rpc_called);

    /* Wait until wamp session is closed. */

    ready_to_exit.get_future().wait();
    return 0;
  }
  catch (std::exception& e)
  {
    std::cout << e.what() << std::endl;
    return 1;
  }
}
