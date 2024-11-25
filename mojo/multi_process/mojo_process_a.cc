#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/thread.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "pipe_reader.h"

mojo::ScopedMessagePipeHandle LaunchProcess(
    const base::CommandLine::StringViewType& process) {
  // Under the hood, this is essentially always an OS pipe (domain socket pair,
  // Windows named pipe, Fuchsia channel, etc).
  mojo::PlatformChannel channel;
  mojo::OutgoingInvitation invitation;

  // Attach a message pipe to be extracted by the receiver. The other end of the
  // pipe is returned for us to use locally.
  mojo::ScopedMessagePipeHandle pipe =
      invitation.AttachMessagePipe("zotrus pipe");

  LOG(INFO) << "pipe before launch process : " << pipe->value();

  base::LaunchOptions options;

  base::CommandLine command_line(
      base::FilePath(FILE_PATH_LITERAL("mojo_process_b.exe")));

  channel.PrepareToPassRemoteEndpoint(&options, &command_line);
  base::Process child_process = base::LaunchProcess(command_line, options);
  channel.RemoteProcessLaunchAttempted();

  mojo::OutgoingInvitation::Send(
      std::move(invitation), child_process.Handle(),
      channel.TakeLocalEndpoint(),
      base::BindRepeating(
          [](const std::string& error) { LOG(ERROR) << error; }));
  return pipe;
}

int main(int argc, char** argv) {
  // 初始化CommandLine，DataPipe 依赖它
  base::CommandLine::Init(argc, argv);
  mojo::core::Configuration mojo_config;
  mojo_config.is_broker_process = true;
  // 初始化 mojo
  mojo::core::Init(mojo_config);
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
  logging::SetLogPrefix("producer");

  LOG(INFO) << base::CommandLine::ForCurrentProcess()->GetCommandLineString();

  // 初始化 Mojo 的IPC支持，只有初始化后进程间的Mojo通信才能有效
  // 这个对象要保证一直存活，否则IPC通信就会断开
  mojo::core::ScopedIPCSupport ipc_support(
      ipc_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  // 创建消息循环
  base::SingleThreadTaskExecutor main_thread_task_executor;
  base::RunLoop run_loop;

  // 启动另一个进程
  auto pipe = LaunchProcess(L"mojo_process_b");
  zotrus::sendMessage(pipe.get(), "mojo_process_a", "hello world");

  run_loop.Run();

  return 0;
}
