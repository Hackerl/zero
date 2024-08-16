#include <zero/concurrent/channel.h>

DEFINE_ERROR_CATEGORY_INSTANCES(
    zero::concurrent::TrySendError,
    zero::concurrent::SendError,
    zero::concurrent::TryReceiveError,
    zero::concurrent::ReceiveError,
    zero::concurrent::ChannelError
)
