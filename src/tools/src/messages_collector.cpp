/*
Abstract:
  Messages collector implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//common includes
#include <messages_collector.h>
//std includes
#include <list>
//boost includes
#include <boost/algorithm/string/join.hpp>

namespace
{
  class MessagesCollectorImpl : public Log::MessagesCollector
  {
  public:
    MessagesCollectorImpl()
    {
    }

    virtual void AddMessage(const String& message)
    {
      Messages.push_back(message);
    }

    virtual uint_t CountMessages() const
    {
      return static_cast<uint_t>(Messages.size());
    }

    virtual String GetMessages(Char delimiter) const
    {
      return boost::join(Messages, String(1, delimiter));
    }

  private:
    std::list<String> Messages;
  };
}

namespace Log
{
  MessagesCollector::Ptr MessagesCollector::Create()
  {
    return MessagesCollector::Ptr(new MessagesCollectorImpl());
  }
}
