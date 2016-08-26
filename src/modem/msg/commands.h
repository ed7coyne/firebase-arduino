#ifndef MODEM_MSG_COMMANDS_H
#define MODEM_MSG_COMMANDS_H

#include "Firebase.h"
#include "modem/command.h"
#include "modem/output-stream.h"
#include "modem/input-stream.h"

namespace firebase {
namespace modem {

struct Argument {
  String name;
  String value;
  Argument(const String& name_, const String& value_) 
      : name(name_), value(value_) {
    name.toLowerCase();
  }

  // Interpret value as bool.
  bool BoolValue();

  // Interpret value as int.
  int IntValue();
}

struct AddressedMessage {
  // This is std::string even though most things are String here to ease
  // our handing it off to the library.
  std::vector<std::string> registration_ids;
  String topic;

  FirebaseCloudMessage message;
}

class BeginMsgCommand {
 public:
  bool execute(const String& command, InputStream* in, OutputStream* out);

  // This can only be called once, only after execute.
  std::unique_ptr<FirebaseCloudMessaging> fcm();
  const std::vector<Argument>& default_arguments();

 private:
  std::unique_ptr<FirebaseCloudMessaging> fcm_;
  std::vector<Argument> default_arguments_;
};

class MsgCommand {
 public:
  MsgCommand(const std::vector<Argument>& default_arguments, AddressedMessage* message) 
    : default_arguments_(default_arguments), message_(message) {}

  bool execute(const String& command, InputStream* in, OutputStream* out);

 private:
  // Apply an arg to the message.
  bool UpdateMessage(const Argument& arg);

  const std::vector<Argument>& default_arguments_;
  AddressedMessage* message_;

};

class NotificationCommand {
 public:
  NotificationCommand(AddressedMessage* message) 
    : message_(message) {}

  bool execute(const String& command, InputStream* in, OutputStream* out);

 private:
  AddressedMessage* message_;
};

class AddDataCommand {
 public:
  AddDataCommand(AddressedMessage* message) 
    : message_(message) {}

  bool execute(const String& command, InputStream* in, OutputStream* out);

 private:
  AddressedMessage* message_;
};

class SendMsgCommand {
 public:
  SendMsgCommand(const AddressedMessage& message, FirebaseCloudMessaging* fcm) 
    : message_(message), fcm_(fcm) {}

  bool execute(const String& command, InputStream* in, OutputStream* out);

 private:
  const AddressedMessage& message_;
  FirebaseCloudMessaging* fcm_;
};


}  // modem
}  // firebase

#endif //MODEM_MSG_COMMANDS_H
