#include "pipe_reader.h"

#include "mojo/public/c/system/types.h"

namespace zotrus {

bool sendMessage(mojo::MessagePipeHandle handle,
                 const std::string& sender,
                 const std::string kMessage) {
  LOG(INFO) << "send pipe valid : " << handle.is_valid();
  auto result = mojo::WriteMessageRaw(handle, kMessage.data(), kMessage.size(),
                                      nullptr, 0, MOJO_WRITE_DATA_FLAG_NONE);
  DCHECK_EQ(result, MOJO_RESULT_OK);
  LOG(INFO) << sender << " send: " << kMessage;

  return result == MOJO_RESULT_OK;
}

MojoResult ReceiveMessage(mojo::MessagePipeHandle handle,
                          const std::string& receiver,
                          std::string& out_msg) {
  std::vector<uint8_t> data;
  auto result =
      mojo::ReadMessageRaw(handle, &data, nullptr, MOJO_READ_DATA_FLAG_NONE);
  if (result == MOJO_RESULT_OK) {
    data.push_back('\0');  // for string
    std::string str(reinterpret_cast<char*>(data.data()));
    LOG(INFO) << receiver << " receive msg: " << str;
    out_msg = str;
  }

  return result;
}

PipeReader::PipeReader(mojo::ScopedMessagePipeHandle pipe)
    : pipe_(std::move(pipe)),
      watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC) {
  // NOTE: base::Unretained is safe because the callback can never be run
  // after SimpleWatcher destruction.
  watcher_.Watch(
      pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      base::BindRepeating(&PipeReader::OnReadable, base::Unretained(this)));
}

PipeReader::~PipeReader() {}

void PipeReader::OnReadable(MojoResult result) {
  while (result == MOJO_RESULT_OK) {
    std::string msg;
    result = ReceiveMessage(pipe_.get(), "receiver", msg);
  }
}

}  // namespace zotrus
