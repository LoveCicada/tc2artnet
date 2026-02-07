
#include "mtcreceiver.h"

#define _CRT_SECURE_NO_WARNINGS 1

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/FileSink.h"


namespace {
constexpr int FRAME_MS_MIN = 30;
constexpr int FRAME_MS_MAX = 45;
}

static TimecodeFrame bits_to_frame(const uint8_t bits[8]);


MTCReceiver::MTCReceiver(QObject *parent)
  :QObject(parent)
{
  InitLog();
  connect(&_midi_dev, &MIDIDev::midiMessage, this, &MTCReceiver::_processMidiMessage);
}


MTCReceiver::~MTCReceiver()
{
  stop();
  UnitLog();
}


void MTCReceiver::setDevice(int index)
{
  if(index == -1 && index != _current_device)
  {
    // No device selected
    stop();
    _current_device = -1;
  }

  if(deviceList().size() > 0 &&
     index >= 0 &&
     index < deviceList().size() &&
     index != _current_device)
  {
    stop();
    _current_device = index;
  }
}


void MTCReceiver::start()
{
  stop();
  reset_qf_vars();

  if(deviceList().size() == 0)
  {
    statusChanged(true, "no MIDI devices");
    return;
  }
  else if(_current_device < 0 || _current_device >= deviceList().size())
  {
    statusChanged(true, "no MIDI device selected");
    return;
  }
  else if(!_midi_dev.open(_current_device))
  {
    statusChanged(true, "cannot open MIDI device");
    return;
  }

  statusChanged(false, "listening");
}


void MTCReceiver::stop()
{
  _midi_dev.close();
  statusChanged(true, "stopped");
}


void MTCReceiver::_processMidiMessage(int time_ms, int status, int data1, int data2)
{
  if(status != 0xF1)
    return;

  data1 &= 0x7F;

  int qf_num = data1 >> 4;
  uint8_t qf_data = data1 & 0xF;
  int dt = time_ms - _last_qf_time;

  if(dt < 0) dt = 0;

  // Remember: we only get a complete assembled frame 7/8ths of a frame
  // after the frame actually happened. As time travel isn't yet supported
  // by Qt, we have to increment the frame to keep in sync.

  if(qf_num == 7 && _last_qf_num == 6)
    _last_frame = bits_to_frame(_qf_bits);

  // On QF 0, we transmit the last assembled frame + 2
  if(qf_num == 0 && _last_qf_num == 7 &&
     dt > (FRAME_MS_MIN*2) && dt < (FRAME_MS_MAX*2))
  {
    TimecodeFrame frame = timecode_frame_add(_last_frame, 2);
    emit newFrame(frame);
    
    // Log MTC timecode
    quill::Logger* logger = quill::Frontend::get_logger("root");
    LOG_INFO(logger, "MTC hh:mm:ss.f: {}:{}:{}.{}, rate:{}", 
        frame.hours, frame.minutes, frame.seconds, frame.frames, static_cast<int8_t>(frame.type));
  }

  // On QF 4, we transmit the last assembled frame + 3
  if(qf_num == 4 && _last_qf_num == 3 &&
     dt > FRAME_MS_MIN && dt < FRAME_MS_MAX)
  {
    TimecodeFrame frame = timecode_frame_add(_last_frame, 3);
    emit newFrame(frame);
    
    // Log MTC timecode
    quill::Logger* logger = quill::Frontend::get_logger("root");
    LOG_INFO(logger, "MTC hh:mm:ss.f: {}:{}:{}.{}, rate:{}", 
        frame.hours, frame.minutes, frame.seconds, frame.frames, static_cast<int8_t>(frame.type));
  }

  if(qf_num == 0)
    _last_qf_time = time_ms;

  if(qf_num == 0 || qf_num == (_last_qf_num + 1))
  {
    _last_qf_num = qf_num;
    _qf_bits[qf_num] = qf_data;
  }
}


void MTCReceiver::reset_qf_vars()
{
  _last_qf_num = -1;
  _last_qf_time = 0;

  for(int i = 0; i < 8; i++)
    _qf_bits[i] = 0;

  timecode_frame_reset(_last_frame);
}


void MTCReceiver::InitLog()
{
    quill::Backend::start();

    //Fronted
    auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(
        "trivial_logging.log",
        []()
    {
        quill::FileSinkConfig cfg;
        cfg.set_open_mode('w');
        cfg.set_filename_append_option(quill::FilenameAppendOption::StartDateTime);
        return cfg;
    }(),
        quill::FileEventNotifier{}
    );

    quill::Logger* logger = quill::Frontend::create_or_get_logger(
        "root", std::move(file_sink)
    );

}

void MTCReceiver::UnitLog()
{
    quill::Backend::stop();
}


static TimecodeFrame bits_to_frame(const uint8_t bits[8])
{
  TimecodeFrame frame;

  frame.frames = bits[0] + (bits[1] << 4);
  frame.seconds = bits[2] + (bits[3] << 4);
  frame.minutes = bits[4] + (bits[5] << 4);
  frame.hours = bits[6] + ((bits[7] & 1) << 4);
  frame.type = static_cast<TimecodeType>((bits[7] >> 1) & 3);

  return frame;
}
