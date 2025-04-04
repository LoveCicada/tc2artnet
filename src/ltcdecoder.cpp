
#include "ltcdecoder.h"
#include <ltc.h>

#define _CRT_SECURE_NO_WARNINGS 1

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/FileSink.h"

#define GETLTC static_cast<LTCDecoder *>(_private)


LTCDecoder::LTCDecoder(QObject *parent)
  :QIODevice(parent)
{
    InitLog();

  int apv = 1920;
  int queue_size = 32;
  _private = ltc_decoder_create(apv, queue_size);

  setOpenMode(QIODevice::WriteOnly);
}


LTCDecoder::~LTCDecoder()
{
  LTCDecoder *ltc = GETLTC;
  ltc_decoder_free(ltc);
  _private = nullptr;

  UnitLog();
}


qint64 LTCDecoder::writeData(const char *data, qint64 len)
{
  static int last_frame = 0;
  static int frame_rate_g = 0;
  static int frame_rate = 0;

  LTCDecoder *ltc = GETLTC;
  LTCFrameExt ltc_frame;
  SMPTETimecode ltc_stime;
  TimecodeFrame frame;

  ltc_decoder_write(ltc, (ltcsnd_sample_t *)data, len, 0);

  while(ltc_decoder_read(ltc, &ltc_frame))
  {
    if(ltc_frame.reverse)
      continue;

    ltc_frame_to_time(&ltc_stime, &ltc_frame.ltc, 0);

    frame.hours = ltc_stime.hours;
    frame.minutes = ltc_stime.mins;
    frame.seconds = ltc_stime.secs;
    frame.frames = ltc_stime.frame;
    frame.type = TimecodeType::SMPTE30;

    if(frame.frames == 0 &&
       (last_frame == 29 || last_frame == 24 || last_frame == 23))
    {
      // Could last_frame be the frame rate? It could very well could very well be.

      int r = last_frame + 1;

      if(r == frame_rate_g) // "frame rate guess"
      {
        // Same rate twice in a row, accept it
        frame_rate = r;
        frame_rate_g = 0;
      }
      else if(r != frame_rate)
      {
        // Possible change of frame rate, or first run
        if(r > frame_rate || frame_rate == 0)
        {
          frame_rate = r;
          frame_rate_g = 0;
        }
        else
        {
          frame_rate_g = r;
        }
      }
      else
      {
        frame_rate_g = 0;
      }
    }

    last_frame = frame.frames;

    if(ltc_frame.ltc.dfbit)
      frame.type = TimecodeType::DF30;
    else if(frame_rate == 25)
      frame.type = TimecodeType::EBU25;
    else if(frame_rate == 24)
      frame.type = TimecodeType::Film24;

    newFrame(frame);

    // Obtain existing logger to log
    quill::Logger* logger = quill::Frontend::get_logger("root");
    LOG_INFO(logger, "hh:mm:ss.f: {}:{}:{}.{}, rate:{}", 
        frame.hours, frame.minutes, frame.seconds, frame.frames, static_cast<int8_t>(frame.type));

    /* //example
    int i = 1, j = 2;
    double k = 3.1;
    LOG_INFO(logger, "Logging int:{}, int:{}, double:{}", i, j, k);*/

  }

  return len;
}

void LTCDecoder::InitLog()
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

void LTCDecoder::UnitLog()
{
    quill::Backend::stop();
}
