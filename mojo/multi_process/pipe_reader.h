#ifndef DEMOS_PIPE_READER_H
#define DEMOS_PIPE_READER_H

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
#include "mojo/public/cpp/system/message.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace zotrus {

bool sendMessage(mojo::MessagePipeHandle handle,
                 const std::string& sender,
                 const std::string kMessage);

MojoResult ReceiveMessage(mojo::MessagePipeHandle handle,
                          const std::string& receiver,
                          std::string& out_msg);

class PipeReader {
 public:
  PipeReader(mojo::ScopedMessagePipeHandle pipe);
  ~PipeReader();

 private:
  void OnReadable(MojoResult result);
  mojo::ScopedMessagePipeHandle pipe_;
  mojo::SimpleWatcher watcher_;
};

}  // namespace zotrus

#endif  // DEMOS_PIPE_READER_H

// mojo::MessagePipe pipe;
// PipeReader reader(std::move(pipe.handle0));

// // Written messages will asynchronously end up in |reader.messages_|.
// WriteABunchOfStuff(pipe.handle1.get());
