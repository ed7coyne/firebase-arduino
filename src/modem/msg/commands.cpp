#include "modem/db/commands.h"

namespace firebase {
namespace modem {
namespace {

constexpr int kIterationsToReadBody = 1000;

// Sorts vector by name as well for easy comparison.
bool ParseArguments(const String& data, std::vector<Argument>* args) {
  // last_space_index tracks the start of our argument, while space_index tracks the end of it.
  int space_index = 0, last_space_index = -1;
  while (space_index > -1) {
    space_index = data.indexOf(' ', last_space_index);
    if (space_index > -1) {
      int equals_index = data.indexOf('=', last_space_index);
      if (equals_index == -1 // There is no equals, syntax error.
          || equals_index > space_index) { // There is a space in this argument, syntax error.
        return false;
      }
      args.emplace_back(
          data.substring(last_space_index + 1, equals_index),
          data.substring(equals_index + 1, space_index));

      last_space_index = space_index;
    }
  }
  std::sort(args->begin(), args->end(),
      [](Argument a, Argument b) { return a.name.compare(b.name); });
  return true;  
}

}  // namespace

bool Argument::BoolValue() const {
  String boolstr = value();
  boolstr.toLowerCase();
  return boolstr == "true";
}

int Argument::IntValue() {
  return atoi(value().c_str()); 
}

bool BeginMsgCommand::execute(const String& command,
                              InputStream* in, OutputStream* out) {
  if (in == nullptr || out == nullptr) {
    return false;
  }

  if (command != "BEGIN_MSG") {
    return false;
  }

  String server_key;

  String data(in->readLine());

  int space_index = data.indexOf(' ');
  if (space_index == -1) {
    // server_key only.
    server_key = data;
  } else {
    // server key and arguments.
    server_key = data.substring(0, space_index);
    
    // TODO(edcoyne) at some point lets whitelist our acceptable list of arguments.
    // TODO(edcoyne) we should also validate argument values here, for integers and bools.
    if (!ParseArguments(data.substring(space_index + 1), &default_arguments_)) {
      out->println("-ERROR_PARSING_ARGUMENTS");
      return false;
    }
  }

  if (server_key.length() == 0) {
    out->println("-ERROR_MISSING_SERVER_KEY");
    return false;
  }

  fcm_.reset(new FirebaseCloudMessaging(server_key));

  out->println("+OK");
  return true;
}

std::unique_ptr<FirebaseCloudMessaging> BeginMsgCommand::fcm() {
  return std::move(fcm_);
}

const std::vector<Argument>& BeginMsgCommand::default_arguments() {
  return default_arguments_;
}


bool MsgCommand::execute(const String& command, InputStream* in, OutputStream* out) {
  if (in == nullptr || out == nullptr) {
    return false;
  }

  if (command != "MSG") {
    return false;
  }

  std::Vector<Argument> args;
  if (!ParseArguments(in.readLine(), &args)) {
    out->println("-ERROR_PARSING_ARGUMENTS");
    return false;
  }

  // since both argument lists are sorted we can iterate through both and use the local
  // overrides where available and the defaults otherwise.
  auto args_itr = args.begin();
  auto defaults_itr = default_arguments_.begin();  

  while (args_itr != args.end() && defaults_itr != default_arguments_.end()) {
    // We have both a default and a local override.
    if (args_itr != args.end() && defaults_itr != default_arguments_.end() 
        && (*args_itr).name() == (*defaults_itr).name()) {
      if (!UpdateMessage(*args_itr)) {
        out->println("-ERROR_INVALID_ARGUMENT");
        return false;
      }
      args_itr++;
      defaults_itr++;

    // Either we only have args left, or the args iterator is behind the defaults.
    } else if ((args_itr != args.end() && defaults_itr == default_arguments_.end())
        || (*args_itr).name() < (*defaults_itr).name()) {
      if (!UpdateMessage(*args_itr)) {
        out->println("-ERROR_INVALID_ARGUMENT");
        return false;
      }
      args_itr++;
  
    // Either we only have defaults left, or the defaults iterator is behind the args.
    } else if (args_itr == args.endw
        || (*args_itr).name() > (*defaults_itr).name()) {
      if (!UpdateMessage(*defaults_itr)) {
        out->println("-ERROR_INVALID_ARGUMENT");
        return false;
      }
      defaults_itr++;
    }
  }

}

bool MsgCommand::UpdateMessage(const Argument& arg) {
  if (arg.name() == "registration_ids") {
    int comma_index = 0, last_comma_index = -1;
    while (comma_index > -1) {
      comma_index = arg.value().indexOf(",");
      if (comma_index == -1) { // end of arg.
        message_->registration_ids.push_back(
            arg.value().substring(last_comma_index + 1).c_str());
      } else {
        message_->registration_ids.push_back(
            arg.value().substring(last_comma_index + 1, comma_index).c_str());
      }
    }

  } else if (arg.name() == "topic") {
    message_->topic = arg.value();
  
  } else if (arg.name() == "collapse_key") {
    message_->message.collapse_key = arg.value().c_str();

  } else if (arg.name() == "high_priority") {
    message_->message.high_priority = arg.BoolValue();

  } else if (arg.name() == "delay_while_idle") {
    message_->message.delay_while_idle = arg.BoolValue();

  } else if (arg.name() == "time_to_live") {
    message_->message.time_to_live = arg.IntValue();
  
  } else {
    return false;    
  }

  return true;
}

bool NotificationCommand::execute(const String& command, InputStream* in, OutputStream* out) {
  if (in == nullptr || out == nullptr) {
    return false;
  }

  if (command != "NOTIFICATION") {
    return false;
  }

  int bytes_in_body = 0;
  { // wrap in block to pop data off of stack when we are done.

    String data(in->readLine());

    int last_space = data.lastIndexOf(" ");
    if (last_space || last_space == data.length() - 1) {
      // Either the message is empty or the line ends in a whitespace.
      // it should end in an integer.
      out->println("-ERROR_INCORRECT_FORMAT");
      return false;
    }

    bytes_in_body = atoi(data.substring(last_space).c_str());
    if (bytes_in_body < 0) {
      out->println("-ERROR_INCORRECT_FORMAT");
      return false;
    }

    message_->message.notification.title = data.substring(0, last_space).c_str();
  
  }


  //TODO(edcoyne) Add check to ensure bytes_in_body is under some hardware limits for memory.
  char buffer[bytes_in_body];
  int read = 0;
  // Iterations stop us from waiting forever.
  int iterations = 0;
  while (read < bytes_in_body && iterations++ < kIterationsToReadBody) {
    int latest_read = in->readBytes(buffer, bytes_in_body - read);
    message_->message.notification.body.append(buffer, latest_read);
    read += latest_read;
  }

  if (iterations >= kIterationsToReadBody) {
    out->println("-ERROR_NOT_ENOUGH_DATA_FOR_BODY");
    return false;
  }

  return true; 
}


bool AddDataCommand::execute(const String& command, InputStream* in, OutputStream* out) {
  if (in == nullptr || out == nullptr) {
    return false;
  }

  if (command != "ADD_DATA") {
    return false;
  }

  int bytes_in_body = 0;
  std::string key;
  { // wrap in block to pop data off of stack when we are done.

    String data(in->readLine());

    int last_space = data.lastIndexOf(" ");
    if (last_space || last_space == data.length() - 1) {
      // Either the message is empty or the line ends in a whitespace.
      // it should end in an integer.
      out->println("-ERROR_INCORRECT_FORMAT");
      return false;
    }

    bytes_in_body = atoi(data.substring(last_space).c_str());
    if (bytes_in_body < 0) {
      out->println("-ERROR_INCORRECT_FORMAT");
      return false;
    }

    key = data.substring(0, last_space).c_str();
  }


  std::string value;
  //TODO(edcoyne) Add check to ensure bytes_in_body is under some hardware limits for memory.
  char buffer[bytes_in_body];
  int read = 0;
  // Iterations stop us from waiting forever.
  int iterations = 0;
  while (read < bytes_in_body && iterations++ < kIterationsToReadBody) {
    int latest_read = in->readBytes(buffer, bytes_in_body - read);
    message_->message.notification.body.append(buffer, latest_read);
    read += latest_read;
  }

  if (iterations >= kIterationsToReadBody) {
    out->println("-ERROR_NOT_ENOUGH_DATA_FOR_BODY");
    return false;
  }

  message_->message.data.emplace_back(key, value);
  return true; 
}

bool AddDataCommand::execute(const String& command, InputStream* in, OutputStream* out) {
  if (in == nullptr || out == nullptr) {
    return false;
  }

  if (command != "SEND_MSG") {
    return false;
  }

  // TODO(edcoyne): our library currently does not support sending to users and topics but
  // I think the Web api does. We should fix that.
  if (!message_.topic.empty()) {
    FirebaseError error = fcm_->SendMessageToTopic(message_.topic.c_str(), message_.message);
    if (error) {
      out->print("-FAIL ");
      out->println(error.message());
      return false;
    }

  } else if (message_.topic.registration_ids.size() == 1) {
    FirebaseError error = fcm_->SendMessageToUser(message_.registration_ids[0],
                                                  message_.message);
    if (error) {
      out->print("-FAIL ");
      out->println(error.message());
      return false;
    }

  } else if (message_.topic.registration_ids.size() > 0) {
    FirebaseError error = fcm_->SendMessageToUsers(message_.registration_ids,
                                                   message_.message);
    if (error) {
      out->print("-FAIL ");
      out->println(error.message());
      return false;
    }
  }
  return true;
}

}  // modem
}  // firebase
