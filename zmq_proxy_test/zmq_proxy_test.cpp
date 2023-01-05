//  Asynchronous client-to-server (DEALER to ROUTER)
   //
   //  While this example runs in a single process, that is to make
   //  it easier to start and stop the example. Each task has its own
   //  context and conceptually acts as a separate process.

#include <vector>
#include <thread>
#include <memory>
#include <functional>
#include <iostream>
#include <random>
#include <cstdlib>
#include <chrono>
#include <mutex>

using namespace std::chrono_literals;

#include <zmq.h>
#include <zmq.hpp>
#include <zmq_addon.hpp>

const std::string scan_filter = "scan";
const std::string rcv_filter = "rcv";

const std::string inproc_pubsub_addr = "inproc://server_pubsub";

std::mutex stdout_mtx;

void print(std::string str) {
	std::lock_guard<std::mutex> stdout_guard(stdout_mtx);
	std::cout << str << std::endl;
}

class scanner {
public:
	scanner(zmq::context_t& ctx) : ctx_(ctx) {
		std::thread([&] () {
			zmq::socket_t scan_pub(ctx_, zmq::socket_type::pub);
			scan_pub.set(zmq::sockopt::linger, 0);
			scan_pub.connect(inproc_pubsub_addr);

			while (is_working_) {
				zmq::message_t msg(std::string("frequency_info"));
				scan_pub.send(zmq::buffer(scan_filter), zmq::send_flags::sndmore);
				scan_pub.send(msg, zmq::send_flags::none);
				print("Scanner sent frequency_info");
				std::this_thread::sleep_for(1s);
			}

			scan_pub.close();
		}).detach();
	}

	~scanner() {
		is_working_ = false;
	}
private:
	zmq::context_t& ctx_;

	std::atomic_bool is_working_{ true };
};

class receiver {
public:
	receiver(zmq::context_t& ctx) : ctx_(ctx) {
		std::thread([&]() {
			zmq::socket_t rcv_pub(ctx_, zmq::socket_type::pub);
			rcv_pub.set(zmq::sockopt::linger, 0);
			rcv_pub.connect(inproc_pubsub_addr);

			while (is_working_) {
				zmq::message_t msg(std::string("inroute_statistics"));
				rcv_pub.send(zmq::buffer(rcv_filter), zmq::send_flags::sndmore);
				rcv_pub.send(msg, zmq::send_flags::none);
				print("Receiver sent inroute_statistics");
				std::this_thread::sleep_for(1s);
			}

			rcv_pub.close();
		}).detach();
	}
private:
	zmq::context_t& ctx_;
	std::atomic_bool is_working_{ true };
};

class server {
public:
	server(zmq::context_t& ctx) : ctx_(ctx), scan_(ctx), rcv_(ctx) {
		client_pub_sock = std::make_unique<zmq::socket_t>(ctx_, ZMQ_PUB);
		client_pub_sock->set(zmq::sockopt::linger, 0);
		client_pub_sock->bind("tcp://127.0.0.1:5555");

		inproc_sub_sock = std::make_unique<zmq::socket_t>(ctx_, ZMQ_SUB);
		inproc_sub_sock->set(zmq::sockopt::linger, 0);
		inproc_sub_sock->set(zmq::sockopt::subscribe, "");
		inproc_sub_sock->bind(inproc_pubsub_addr);

		auto err = zmq_proxy(*inproc_sub_sock, *client_pub_sock, nullptr);
	}

	~server() {
		client_pub_sock->close();
		inproc_sub_sock->close();
	}
private:

	zmq::context_t& ctx_;

	std::unique_ptr<zmq::socket_t> client_pub_sock;
	std::unique_ptr<zmq::socket_t> inproc_sub_sock;

	scanner scan_;
	receiver rcv_;
};

class client {
public:
	client(zmq::context_t& ctx) : ctx_(ctx) {
		cl_sock_ = std::make_unique<zmq::socket_t>(ctx_, ZMQ_SUB);
		cl_sock_->set(zmq::sockopt::linger, 0);
		cl_sock_->set(zmq::sockopt::subscribe, rcv_filter);
		cl_sock_->connect("tcp://127.0.0.1:5555");

		std::thread([&] () {
			while (is_working_) {
				std::vector<zmq::message_t> msg;
				if (zmq::recv_multipart(*cl_sock_, std::back_inserter(msg))) {
					print("Client received " + msg[1].to_string());
				}
				std::this_thread::sleep_for(100ms);
			}
		}).detach();
	}

	~client() {
		is_working_ = false;
		std::this_thread::sleep_for(200ms);
		cl_sock_->close();
	}
private:
	std::unique_ptr<zmq::socket_t> cl_sock_;
	zmq::context_t& ctx_;

	std::atomic_bool is_working_{ true };
};

int main() {

	zmq::context_t ctx_clnt;
	zmq::context_t ctx_srv;

	client clnt(ctx_clnt);
	server srv(ctx_srv);

	getchar();

	return 0;
}
