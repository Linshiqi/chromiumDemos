#include <cstdint>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/thread.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/message_pipe.h"

bool sendMessage(mojo::MessagePipeHandle handle,
                 const std::string& sender,
                 const std::string kMessage) {
  auto result = mojo::WriteMessageRaw(handle, kMessage.data(), kMessage.size(),
                                      nullptr, 0, MOJO_WRITE_DATA_FLAG_NONE);
  DCHECK_EQ(result, MOJO_RESULT_OK);
  LOG(INFO) << sender << " send: " << kMessage;

  return result == MOJO_RESULT_OK;
}

void ReceiveMessage(mojo::MessagePipeHandle handle,
                    const std::string& receiver,
                    std::string& out_msg) {
  std::vector<uint8_t> data;
  auto result =
      mojo::ReadMessageRaw(handle, &data, nullptr, MOJO_READ_DATA_FLAG_NONE);
  DCHECK_EQ(result, MOJO_RESULT_OK);
  data.push_back('\0');  // for string
  std::string str(reinterpret_cast<char*>(data.data()));
  LOG(INFO) << receiver << " receive msg: " << str;

  out_msg = str;
}

// message pipe is bidirectional
void testMessagePipe() {
  LOG(INFO) << "test MessagePipe";

  // 使用C++接口创建一条MessagePipe
  mojo::MessagePipe pipe;
  // 使用C++接口发送一条消息
  {
    const std::string kMessage = "Hello";
    sendMessage(pipe.handle0.get(), "pipe.handle0", kMessage);
  }

  // 使用C++接口接收一条消息
  {
    if (pipe.handle1->QuerySignalsState().readable()) {
      std::string received_msg;
      ReceiveMessage(pipe.handle1.get(), "pipe.handle1", received_msg);
    }
  }

  // 验证双向通信，回复一个消息
  {
    const std::string reply_msg = "Hi";
    sendMessage(pipe.handle1.get(), "pipe.handle1", reply_msg);

    if (pipe.handle0->QuerySignalsState().readable()) {
      std::string received_msg;
      ReceiveMessage(pipe.handle0.get(), "pipe.handle0", received_msg);
    }
  }
}

// data pipe is one directional
void testDataPipe() {
  LOG(INFO) << "test DatePipe";

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  mojo::CreateDataPipe(nullptr, producer, consumer);

  // Send data
  {
    const std::string message = "hello";
    auto data_to_sent = base::as_bytes(base::make_span(message));
    auto result = producer->WriteAllData(data_to_sent);
    DCHECK_EQ(result, MOJO_RESULT_OK);
    LOG(INFO) << "send: " << message;
  }

  // Receive data
  {
    if (consumer->QuerySignalsState().readable()) {
      size_t num_bytes;
      auto result = consumer->ReadData(MOJO_READ_DATA_FLAG_QUERY,
                                       base::span<uint8_t>(), num_bytes);
      CHECK_EQ(result, MOJO_RESULT_OK);

      std::vector<uint8_t> data(num_bytes);
      base::span<uint8_t> data_span(data);
      result =
          consumer->ReadData(MOJO_READ_DATA_FLAG_NONE, data_span, num_bytes);
      DCHECK_EQ(result, MOJO_RESULT_OK);
      data.push_back('\0');  // for string
      std::string str(reinterpret_cast<char*>(data.data()));
      LOG(INFO) << "receive msg: " << str;
    }
  }
}

int main(int argc, char** argv) {
  // 初始化CommandLine，DataPipe 依赖它
  base::CommandLine::Init(argc, argv);
  // 初始化 mojo
  mojo::core::Init();
  // 创建一个线程，用于Mojo内部收发数据
  base::Thread ipc_thread("ipc!");
  ipc_thread.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));

#if defined(OS_WIN)
  logging::LoggingSettings logging_setting;
  logging_setting.logging_dest = logging::LOG_TO_STDERR;
  logging::SetLogItems(true, true, false, false);
  logging::InitLogging(logging_setting);
#endif

  // 初始化 Mojo 的IPC支持，只有初始化后进程间的Mojo通信才能有效
  // 这个对象要保证一直存活，否则IPC通信就会断开
  mojo::core::ScopedIPCSupport ipc_support(
      ipc_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  testMessagePipe();

  testDataPipe();

  // 创建消息循环
  base::SingleThreadTaskExecutor main_thread_task_executor;
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  return 0;
}
