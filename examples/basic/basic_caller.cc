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
#include <thread>

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

int main(int argc, char** argv)
{
  generate_random_bytes();
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

    auto logon_fut = session->hello(credentials);

    if (logon_fut.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
      throw std::runtime_error("time-out during session logon");

    if(!session->is_open())
      throw std::runtime_error("session logon failed");

    /* Session is now open, call a remote procedure. */
    size_t raw_size = sizeof(random_bytes);
    json_value jbin = json_value::make_binary(random_bytes, raw_size);

    wamp_args call_args;
    call_args.args_list = json_array({jbin});
    using namespace std::chrono;
    const auto interval = std::chrono::nanoseconds(10000);

    while (true) {
      auto start = steady_clock::now();

      // Perform the RPC call
      session->call(
          rpc_uri, {}, call_args,
          [=](wampcc::wamp_session&, wampcc::result_info r) {
              try {
                  // Example of result handling
                  // json_value valRet = r.args.args_list[0];
                  // json_binary bin = valRet.as_binary();
                  // std::cout << "rpc result: " << bin.data() << std::endl;
              } catch (...) {
                  std::cerr << "exception in RPC result handler" << std::endl;
              }
          });

      // Sleep so total loop time ≈ 10 ms
      std::this_thread::sleep_until(start + interval);
    }

    session->call(rpc_uri, {}, call_args,
                  [=, &ready_to_exit](wampcc::wamp_session&, result_info r) {
                    try {
                      // json_value valRet = r.args.args_list[0];
                      // json_binary bin = valRet.as_binary();
                      // assert(jbin == valRet);
                      // std::cout << "rpc result: " << bin.data() << std::endl;
                      // ready_to_exit.set_value();
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
