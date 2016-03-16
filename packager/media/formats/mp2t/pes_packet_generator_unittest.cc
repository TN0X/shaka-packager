// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/media/base/audio_stream_info.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/text_stream_info.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/filters/nal_unit_to_byte_stream_converter.h"
#include "packager/media/formats/mp2t/pes_packet.h"
#include "packager/media/formats/mp2t/pes_packet_generator.h"
#include "packager/media/formats/mp4/aac_audio_specific_config.h"

namespace edash_packager {
namespace media {
namespace mp2t {

using ::testing::_;
using ::testing::DoAll;
using ::testing::SetArgPointee;
using ::testing::Return;

namespace {

// Bogus data for testing.
const uint8_t kAnyData[] = {
  0x56, 0x87, 0x88, 0x33, 0x98, 0xAF, 0xE5,
};

const bool kIsKeyFrame = true;

// Only {Audio,Video}Codec and extra data matter for this test. Other values are
// bogus.
const VideoCodec kH264VideoCodec = VideoCodec::kCodecH264;
const AudioCodec kAacAudioCodec = AudioCodec::kCodecAAC;

// TODO(rkuroiwa): It might make sense to inject factory functions to create
// NalUnitToByteStreamConverter and AACAudioSpecificConfig so that these
// extra data don't need to be copy pasted from other tests.
const uint8_t kVideoExtraData[] = {
    0x01,        // configuration version (must be 1)
    0x00,        // AVCProfileIndication (bogus)
    0x00,        // profile_compatibility (bogus)
    0x00,        // AVCLevelIndication (bogus)
    0xFF,        // Length size minus 1 == 3
    0xE1,        // 1 sps.
    0x00, 0x1D,  // SPS length == 29
    0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4,
    0x2F, 0xF9, 0x7F, 0xF0, 0x00, 0x80, 0x00, 0x91,
    0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA,
    0x60, 0x0F, 0x16, 0x2D, 0x96,
    0x01,        // 1 pps.
    0x00, 0x0A,  // PPS length == 10
    0x68, 0xFE, 0xFD, 0xFC, 0xFB, 0x11, 0x12, 0x13, 0x14, 0x15,
};

// Basic profile.
const uint8_t kAudioExtraData[] = {0x12, 0x10};

const int kTrackId = 0;
const uint32_t kTimeScale = 90000;
const uint64_t kDuration = 180000;
const char kCodecString[] = "avc1";
const char kLanguage[] = "eng";
const uint32_t kWidth = 1280;
const uint32_t kHeight = 720;
const uint32_t kPixelWidth = 1;
const uint32_t kPixelHeight = 1;
const uint16_t kTrickPlayRate = 1;
const uint8_t kNaluLengthSize = 1;
const bool kIsEncrypted = false;

const uint8_t kSampleBits = 16;
const uint8_t kNumChannels = 2;
const uint32_t kSamplingFrequency = 44100;
const uint32_t kMaxBitrate = 320000;
const uint32_t kAverageBitrate = 256000;

class MockNalUnitToByteStreamConverter : public NalUnitToByteStreamConverter {
 public:
  MOCK_METHOD3(Initialize,
               bool(const uint8_t* decoder_configuration_data,
                    size_t decoder_configuration_data_size,
                    bool escape_data));
  MOCK_METHOD4(ConvertUnitToByteStream,
               bool(const uint8_t* sample,
                    size_t sample_size,
                    bool is_key_frame,
                    std::vector<uint8_t>* output));
};

class MockAACAudioSpecificConfig : public mp4::AACAudioSpecificConfig {
 public:
  MOCK_METHOD1(Parse, bool(const std::vector<uint8_t>& data));
  MOCK_CONST_METHOD1(ConvertToADTS, bool(std::vector<uint8_t>* buffer));
};

scoped_refptr<VideoStreamInfo> CreateVideoStreamInfo(VideoCodec codec) {
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kTimeScale, kDuration, codec, kCodecString, kLanguage,
      kWidth, kHeight, kPixelWidth, kPixelHeight, kTrickPlayRate,
      kNaluLengthSize, kVideoExtraData, arraysize(kVideoExtraData),
      kIsEncrypted));
  return stream_info;
}

scoped_refptr<AudioStreamInfo> CreateAudioStreamInfo(AudioCodec codec) {
  scoped_refptr<AudioStreamInfo> stream_info(new AudioStreamInfo(
      kTrackId, kTimeScale, kDuration, codec, kCodecString,
      kLanguage, kSampleBits, kNumChannels, kSamplingFrequency, kMaxBitrate,
      kAverageBitrate, kAudioExtraData, arraysize(kAudioExtraData),
      kIsEncrypted));
  return stream_info;
}

}  // namespace

class PesPacketGeneratorTest : public ::testing::Test {
 protected:
  void UseMockNalUnitToByteStreamConverter(
      scoped_ptr<MockNalUnitToByteStreamConverter>
          mock_nal_unit_to_byte_stream_converter) {
    generator_.converter_ = mock_nal_unit_to_byte_stream_converter.Pass();
  }

  void UseMockAACAudioSpecificConfig(
      scoped_ptr<MockAACAudioSpecificConfig> mock) {
    generator_.adts_converter_ = mock.Pass();
  }

  PesPacketGenerator generator_;
};

TEST_F(PesPacketGeneratorTest, InitializeVideo) {
  scoped_refptr<VideoStreamInfo> stream_info(
      CreateVideoStreamInfo(kH264VideoCodec));
  EXPECT_TRUE(generator_.Initialize(*stream_info));
}

TEST_F(PesPacketGeneratorTest, InitializeVideoNonH264) {
  scoped_refptr<VideoStreamInfo> stream_info(
      CreateVideoStreamInfo(VideoCodec::kCodecVP9));
  EXPECT_FALSE(generator_.Initialize(*stream_info));
}

TEST_F(PesPacketGeneratorTest, InitializeAudio) {
  scoped_refptr<AudioStreamInfo> stream_info(
      CreateAudioStreamInfo(kAacAudioCodec));
  EXPECT_TRUE(generator_.Initialize(*stream_info));
}

TEST_F(PesPacketGeneratorTest, InitializeAudioNonAac) {
  scoped_refptr<AudioStreamInfo> stream_info(
      CreateAudioStreamInfo(AudioCodec::kCodecOpus));
  EXPECT_FALSE(generator_.Initialize(*stream_info));
}

// Text is not supported yet.
TEST_F(PesPacketGeneratorTest, InitializeTextInfo) {
  scoped_refptr<TextStreamInfo> stream_info(
      new TextStreamInfo(kTrackId, kTimeScale, kDuration, kCodecString,
                         kLanguage, std::string(), kWidth, kHeight));
  EXPECT_FALSE(generator_.Initialize(*stream_info));
}

TEST_F(PesPacketGeneratorTest, AddVideoSample) {
  scoped_refptr<VideoStreamInfo> stream_info(
      CreateVideoStreamInfo(kH264VideoCodec));
  EXPECT_TRUE(generator_.Initialize(*stream_info));
  EXPECT_EQ(0u, generator_.NumberOfReadyPesPackets());

  scoped_refptr<MediaSample> sample =
      MediaSample::CopyFrom(kAnyData, arraysize(kAnyData), kIsKeyFrame);
  const uint32_t kPts = 12345;
  const uint32_t kDts = 12300;
  sample->set_pts(kPts);
  sample->set_dts(kDts);

  std::vector<uint8_t> expected_data(kAnyData, kAnyData + arraysize(kAnyData));

  scoped_ptr<MockNalUnitToByteStreamConverter> mock(
      new MockNalUnitToByteStreamConverter());
  EXPECT_CALL(*mock,
              ConvertUnitToByteStream(_, arraysize(kAnyData), kIsKeyFrame, _))
      .WillOnce(DoAll(SetArgPointee<3>(expected_data), Return(true)));

  UseMockNalUnitToByteStreamConverter(mock.Pass());

  EXPECT_TRUE(generator_.PushSample(sample));
  EXPECT_EQ(1u, generator_.NumberOfReadyPesPackets());
  scoped_ptr<PesPacket> pes_packet = generator_.GetNextPesPacket();
  ASSERT_TRUE(pes_packet);
  EXPECT_EQ(0u, generator_.NumberOfReadyPesPackets());

  EXPECT_EQ(0xe0, pes_packet->stream_id());
  EXPECT_EQ(kPts, pes_packet->pts());
  EXPECT_EQ(kDts, pes_packet->dts());
  EXPECT_EQ(expected_data, pes_packet->data());

  EXPECT_TRUE(generator_.Flush());
}

TEST_F(PesPacketGeneratorTest, AddVideoSampleFailedToConvert) {
  scoped_refptr<VideoStreamInfo> stream_info(
      CreateVideoStreamInfo(kH264VideoCodec));
  EXPECT_TRUE(generator_.Initialize(*stream_info));
  EXPECT_EQ(0u, generator_.NumberOfReadyPesPackets());

  scoped_refptr<MediaSample> sample =
      MediaSample::CopyFrom(kAnyData, arraysize(kAnyData), kIsKeyFrame);

  std::vector<uint8_t> expected_data(kAnyData, kAnyData + arraysize(kAnyData));
  scoped_ptr<MockNalUnitToByteStreamConverter> mock(
      new MockNalUnitToByteStreamConverter());
  EXPECT_CALL(*mock,
              ConvertUnitToByteStream(_, arraysize(kAnyData), kIsKeyFrame, _))
      .WillOnce(Return(false));

  UseMockNalUnitToByteStreamConverter(mock.Pass());

  EXPECT_FALSE(generator_.PushSample(sample));
  EXPECT_EQ(0u, generator_.NumberOfReadyPesPackets());
  EXPECT_TRUE(generator_.Flush());
}

TEST_F(PesPacketGeneratorTest, AddAudioSample) {
  scoped_refptr<AudioStreamInfo> stream_info(
      CreateAudioStreamInfo(kAacAudioCodec));
  EXPECT_TRUE(generator_.Initialize(*stream_info));
  EXPECT_EQ(0u, generator_.NumberOfReadyPesPackets());

  scoped_refptr<MediaSample> sample =
      MediaSample::CopyFrom(kAnyData, arraysize(kAnyData), kIsKeyFrame);

  std::vector<uint8_t> expected_data(kAnyData, kAnyData + arraysize(kAnyData));

  scoped_ptr<MockAACAudioSpecificConfig> mock(new MockAACAudioSpecificConfig());
  EXPECT_CALL(*mock, ConvertToADTS(_))
      .WillOnce(DoAll(SetArgPointee<0>(expected_data), Return(true)));

  UseMockAACAudioSpecificConfig(mock.Pass());

  EXPECT_TRUE(generator_.PushSample(sample));
  EXPECT_EQ(1u, generator_.NumberOfReadyPesPackets());
  scoped_ptr<PesPacket> pes_packet = generator_.GetNextPesPacket();
  ASSERT_TRUE(pes_packet);
  EXPECT_EQ(0u, generator_.NumberOfReadyPesPackets());

  EXPECT_EQ(0xc0, pes_packet->stream_id());
  EXPECT_EQ(expected_data, pes_packet->data());

  EXPECT_TRUE(generator_.Flush());
}

TEST_F(PesPacketGeneratorTest, AddAudioSampleFailedToConvert) {
  scoped_refptr<AudioStreamInfo> stream_info(
      CreateAudioStreamInfo(kAacAudioCodec));
  EXPECT_TRUE(generator_.Initialize(*stream_info));
  EXPECT_EQ(0u, generator_.NumberOfReadyPesPackets());

  scoped_refptr<MediaSample> sample =
      MediaSample::CopyFrom(kAnyData, arraysize(kAnyData), kIsKeyFrame);

  scoped_ptr<MockAACAudioSpecificConfig> mock(new MockAACAudioSpecificConfig());
  EXPECT_CALL(*mock, ConvertToADTS(_)).WillOnce(Return(false));

  UseMockAACAudioSpecificConfig(mock.Pass());

  EXPECT_FALSE(generator_.PushSample(sample));
  EXPECT_EQ(0u, generator_.NumberOfReadyPesPackets());
  EXPECT_TRUE(generator_.Flush());
}

// Because TS has to use 90000 as its timescale, make sure that the timestamps
// are scaled.
TEST_F(PesPacketGeneratorTest, TimeStampScaling) {
  const uint32_t kTestTimescale = 1000;
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kTestTimescale, kDuration, kH264VideoCodec, kCodecString,
      kLanguage, kWidth, kHeight, kPixelWidth, kPixelHeight, kTrickPlayRate,
      kNaluLengthSize, kVideoExtraData, arraysize(kVideoExtraData),
      kIsEncrypted));
  EXPECT_TRUE(generator_.Initialize(*stream_info));

  EXPECT_EQ(0u, generator_.NumberOfReadyPesPackets());

  scoped_refptr<MediaSample> sample =
      MediaSample::CopyFrom(kAnyData, arraysize(kAnyData), kIsKeyFrame);
  const uint32_t kPts = 5000;
  const uint32_t kDts = 4000;
  sample->set_pts(kPts);
  sample->set_dts(kDts);

  scoped_ptr<MockNalUnitToByteStreamConverter> mock(
      new MockNalUnitToByteStreamConverter());
  EXPECT_CALL(*mock,
              ConvertUnitToByteStream(_, arraysize(kAnyData), kIsKeyFrame, _))
      .WillOnce(Return(true));

  UseMockNalUnitToByteStreamConverter(mock.Pass());

  EXPECT_TRUE(generator_.PushSample(sample));
  EXPECT_EQ(1u, generator_.NumberOfReadyPesPackets());
  scoped_ptr<PesPacket> pes_packet = generator_.GetNextPesPacket();
  ASSERT_TRUE(pes_packet);
  EXPECT_EQ(0u, generator_.NumberOfReadyPesPackets());

  // Since 90000 (MPEG2 timescale) / 1000 (input timescale) is 90, the
  // timestamps should be multipled by 90.
  EXPECT_EQ(kPts * 90, pes_packet->pts());
  EXPECT_EQ(kDts * 90, pes_packet->dts());

  EXPECT_TRUE(generator_.Flush());
}

}  // namespace mp2t
}  // namespace media
}  // namespace edash_packager
