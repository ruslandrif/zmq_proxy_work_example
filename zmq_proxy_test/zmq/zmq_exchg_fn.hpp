#pragma once

#include <string>
#include <zmq.hpp>
#include "log/easylogging++.h"

namespace cmn::zmq
{
	using namespace std::string_literals;
	
	const std::string zmq_msg_app = "inproc://msg_app";

	enum class zmq_cmd
	{
		log_app,
	};
	enum class log_type
	{
		info,
		warning,
		error,
	};

	struct log_msg_app
	{
		zmq_cmd cmd{ zmq_cmd::log_app };
		log_type type{ log_type::info };
		size_t msg_sz{ 0 };
		char msg[512];
	};

	static bool send_message_app(void* pContextZmq, std::string strLog, log_type t = log_type::info)
	{
		if (pContextZmq == nullptr)
			return false;

		log_msg_app msg{ zmq_cmd::log_app,t,strLog.size() };

		if (strLog.size() > sizeof msg.msg)
		{
			LOG(WARNING) << "[send_message_app] size message greater than log_msg_app msg buffer."
				<< "msg" << strLog.size()
				<< "buf" << sizeof msg.msg;

			return false;
		}
		strcpy_s(msg.msg, sizeof(msg.msg), strLog.c_str());

		::zmq::message_t req((void*)&msg, sizeof(msg));
		::zmq::const_buffer buf((void*)&msg, sizeof(msg) + strLog.size() + 1);

		auto cntx = (::zmq::context_t*)pContextZmq;
		try
		{
			auto pSubReqRep = ::zmq::socket_t(*cntx, ::zmq::socket_type::pair);
			pSubReqRep.connect(zmq_msg_app);

			if (!pSubReqRep.send(req,::zmq::send_flags::none))
			{
				LOG(ERROR) << "[send_message_app] error send message" << strLog;
				return false;
			}
			return true;
		}
		catch (::zmq::error_t& ex)
		{
			LOG(ERROR) << "[send_message_app]" << ex.what();
		}

		return false;
	}

	inline ::zmq::message_t rcv_zmq_msg(const std::unique_ptr<::zmq::socket_t>& conn, bool dontwait = false)
	{
		if (!conn)
			return {};

		try
		{
			::zmq::message_t msg{};

			auto flag = dontwait ? ::zmq::recv_flags::dontwait : ::zmq::recv_flags::none;
			if (auto rsl = conn->recv(msg, flag))
				if (rsl.value() > 0)
				{
					return msg;
				}			
		}
		catch (std::exception& ex)
		{
			LOG(ERROR) << ex.what();
		}
		return {};
	}
	
	inline std::string GetZmqMsg(const std::unique_ptr<::zmq::socket_t>& conn, bool dontwait = false)
	{
		auto msg = rcv_zmq_msg(conn, dontwait);
		return msg.to_string();
	}

	inline bool send_zmq_msg(const std::unique_ptr<::zmq::socket_t>& conn, ::zmq::message_t& msg, bool dontwait)
	{
		auto flag = dontwait ? ::zmq::send_flags::dontwait : ::zmq::send_flags::none;

		auto result = conn->send(msg, flag);
		if (result && result.value() > 0)
			return true;
		return false;
	}
	
	inline bool SendZmqMsg(const std::unique_ptr<::zmq::socket_t>& conn, std::vector<std::byte>& rep, bool dontwait = false)
	{
		try
		{
			if (!conn || rep.empty())
				return false;

			::zmq::message_t message(rep.data(), rep.size());
			
			return send_zmq_msg(conn,message, dontwait);
		}
		catch (std::exception& ex)
		{
			LOG(ERROR) << ex.what();
			return false;
		}
	}

	inline bool SendZmqMsg(const std::unique_ptr<::zmq::socket_t>& conn, std::string rep, bool dontwait = false)
	{
		try
		{
			if (!conn || rep.empty())
				return false;

			::zmq::message_t message(rep.begin(), rep.end());

			return send_zmq_msg(conn, message, dontwait);
		}
		catch (std::exception& ex)
		{
			LOG(ERROR) << ex.what();
			return false;
		}
	}

}
