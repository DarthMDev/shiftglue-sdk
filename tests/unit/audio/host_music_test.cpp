/**
 * @file        host_music_test.cpp
 * @brief       Host music player track discovery and WAV decoding tests
 * @license     BSD 3-Clause License
 */

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <rex/audio/host_music.h>
#include <rex/filesystem.h>

namespace {

namespace fs = std::filesystem;
using rex::audio::HostMusicPlayer;

void Put16(std::vector<uint8_t>& out, uint16_t value) {
  out.push_back(uint8_t(value));
  out.push_back(uint8_t(value >> 8));
}

void Put32(std::vector<uint8_t>& out, uint32_t value) {
  Put16(out, uint16_t(value));
  Put16(out, uint16_t(value >> 16));
}

void WriteWav(const fs::path& path, uint16_t format, uint16_t channels, uint32_t rate,
              uint16_t bits, const std::vector<uint8_t>& data) {
  std::vector<uint8_t> out;
  out.insert(out.end(), {'R', 'I', 'F', 'F'});
  Put32(out, uint32_t(36 + data.size()));
  out.insert(out.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
  Put32(out, 16);
  Put16(out, format);
  Put16(out, channels);
  Put32(out, rate);
  Put32(out, rate * channels * bits / 8);
  Put16(out, uint16_t(channels * bits / 8));
  Put16(out, bits);
  out.insert(out.end(), {'d', 'a', 't', 'a'});
  Put32(out, uint32_t(data.size()));
  out.insert(out.end(), data.begin(), data.end());
  std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(out.data()),
                                              std::streamsize(out.size()));
}

template <typename Predicate>
bool WaitForMusic(Predicate predicate, std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (!predicate() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return predicate();
}

struct TempFolder {
  fs::path path = fs::temp_directory_path() / "rex_host_music_test";
  TempFolder() {
    fs::remove_all(path);
    fs::create_directories(path / "sub");
  }
  ~TempFolder() {
    std::error_code error;
    fs::remove_all(path, error);
  }
};

}  // namespace

TEST_CASE("Host music decodes mono PCM16 WAV to stereo float", "[audio][host_music]") {
  TempFolder folder;
  std::vector<uint8_t> data;
  for (int16_t sample : {int16_t(0), int16_t(16384), int16_t(-32768)}) {
    Put16(data, uint16_t(sample));
  }
  const auto file = folder.path / "mono.wav";
  WriteWav(file, 1, 1, 22050, 16, data);

  int rate = 0;
  const auto samples = HostMusicPlayer::DecodeForTest(file, rate);
  REQUIRE(rate == 22050);
  REQUIRE(samples.size() == 6);
  CHECK(samples[0] == 0.0f);
  CHECK(samples[2] == 0.5f);
  CHECK(samples[3] == 0.5f);
  CHECK(samples[4] == -1.0f);
}

TEST_CASE("Host music decodes stereo float WAV", "[audio][host_music]") {
  TempFolder folder;
  std::vector<uint8_t> data(4 * sizeof(float));
  const float values[] = {0.25f, -0.25f, 1.0f, -1.0f};
  std::memcpy(data.data(), values, sizeof(values));
  const auto file = folder.path / "float.wav";
  WriteWav(file, 3, 2, 48000, 32, data);

  int rate = 0;
  const auto samples = HostMusicPlayer::DecodeForTest(file, rate);
  REQUIRE(rate == 48000);
  REQUIRE(samples.size() == 4);
  CHECK(samples[0] == 0.25f);
  CHECK(samples[1] == -0.25f);
  CHECK(samples[3] == -1.0f);
}

TEST_CASE("Host music rejects files it cannot decode", "[audio][host_music]") {
  TempFolder folder;
  const auto file = folder.path / "broken.wav";
  std::ofstream(file, std::ios::binary) << "not a wave file";
  int rate = 0;
  CHECK(HostMusicPlayer::DecodeForTest(file, rate).empty());
}

TEST_CASE("Host music finds playable tracks recursively in name order", "[audio][host_music]") {
  TempFolder folder;
  std::vector<uint8_t> data(4);
  WriteWav(folder.path / "b.wav", 1, 1, 44100, 16, data);
  WriteWav(folder.path / "sub" / "a.WAV", 1, 1, 44100, 16, data);
  std::ofstream(folder.path / "notes.txt") << "x";
  std::ofstream(folder.path / "c.mp3") << "x";

  const auto tracks = HostMusicPlayer::FindTracks(folder.path);
  std::vector<fs::path> expected{folder.path / "b.wav"};
  if (HostMusicPlayer::CanDecodeMp3()) expected.push_back(folder.path / "c.mp3");
  expected.push_back(folder.path / "sub" / "a.WAV");
  CHECK(tracks == expected);
}

TEST_CASE("Host music reports a title the ANSI code page cannot hold", "[audio][host_music]") {
  // path::string() threw for a Cyrillic file name on Windows, so the
  // status line crashed the game (pinyon-shift#552). Titles are UTF-8.
  TempFolder folder;
  std::vector<uint8_t> data(4);
  // "Кино - Кукушка" in UTF-8.
  const std::string title =
      "\xD0\x9A\xD0\xB8\xD0\xBD\xD0\xBE - \xD0\x9A\xD1\x83\xD0\xBA\xD1\x83\xD1\x88\xD0\xBA\xD0\xB0";
  WriteWav(folder.path / rex::to_path(title + ".wav"), 1, 1, 44100, 16, data);

  auto& player = HostMusicPlayer::Get();
  REQUIRE(player.Play(folder.path));
  const auto status = player.GetStatus();
  player.Stop();
  CHECK(status.count == 1);
  CHECK(status.title == title);
}

TEST_CASE("Host music advances after resampled tracks finish", "[audio][host_music][.audio-device]") {
  for (const uint32_t rate : {44100u, 48000u}) {
    CAPTURE(rate);
    TempFolder folder;
    std::vector<uint8_t> data(rate / 10 * 2);
    WriteWav(folder.path / "a.wav", 1, 1, rate, 16, data);
    WriteWav(folder.path / "b.wav", 1, 1, rate, 16, data);
    auto& player = HostMusicPlayer::Get();
    player.SetVolume(0.0f);
    player.SetShuffle(false);
    REQUIRE(player.Play(folder.path));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    bool advanced = false;
    while (std::chrono::steady_clock::now() < deadline) {
      if (player.GetStatus().title == "b") {
        advanced = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    INFO("playing=" << player.GetStatus().playing << " title=" << player.GetStatus().title);
    player.Stop();
    CHECK(advanced);
  }
}

TEST_CASE("Host music preserves a paused track through its queued tail", "[audio][host_music][.audio-device]") {
  TempFolder folder;
  WriteWav(folder.path / "a.wav", 1, 1, 48000, 16, std::vector<uint8_t>(48000 / 5 * 2));
  WriteWav(folder.path / "b.wav", 1, 1, 48000, 16, std::vector<uint8_t>(48000 * 2));
  auto& player = HostMusicPlayer::Get();
  player.SetVolume(0.0f);
  player.SetShuffle(false);
  REQUIRE(player.Play(folder.path));
  std::this_thread::sleep_for(std::chrono::milliseconds(60));
  player.SetPaused(true);
  std::this_thread::sleep_for(std::chrono::milliseconds(400));
  const auto paused = player.GetStatus();
  player.SetPaused(false);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (player.GetStatus().title != "b" && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  const auto resumed = player.GetStatus();
  player.Stop();
  CHECK(paused.paused);
  CHECK(paused.title == "a");
  CHECK(resumed.title == "b");
}

TEST_CASE("Independent music playlists resume without taking system ownership", "[audio][host_music][.audio-device]") {
  TempFolder folder;
  fs::create_directories(folder.path / "menu");
  fs::create_directories(folder.path / "race");
  for (const auto* name : {"menu", "race"}) {
    WriteWav(folder.path / name / "a.wav", 1, 1, 48000, 16, std::vector<uint8_t>(48000 * 4));
    WriteWav(folder.path / name / "b.wav", 1, 1, 48000, 16, std::vector<uint8_t>(48000 * 4));
  }
  HostMusicPlayer::Get().Stop();
  REQUIRE(WaitForMusic([] { return !rex::audio::HostMusicHasTheMusic(); }));
  HostMusicPlayer menu, race;
  menu.SetVolume(0.0f);
  race.SetVolume(0.0f);
  REQUIRE(menu.Play(folder.path / "menu", true));
  REQUIRE(race.Play(folder.path / "race", true));
  const bool ready = WaitForMusic([&] { return menu.GetStatus().ready && race.GetStatus().ready; });
  INFO("menu ready/paused=" << menu.GetStatus().ready << "/" << menu.GetStatus().paused
       << " race ready/paused=" << race.GetStatus().ready << "/" << race.GetStatus().paused);
  REQUIRE(ready);
  CHECK(menu.GetStatus().paused);
  CHECK(race.GetStatus().paused);
  CHECK_FALSE(rex::audio::HostMusicHasTheMusic());
  menu.SetPaused(false);
  std::this_thread::sleep_for(std::chrono::milliseconds(1400));
  menu.SetPaused(true);
  race.SetPaused(false);
  std::this_thread::sleep_for(std::chrono::milliseconds(1400));
  race.SetPaused(true);
  CHECK(menu.GetStatus().title == "a");
  CHECK(race.GetStatus().title == "a");
  menu.SetPaused(false);
  CHECK(WaitForMusic([&] { return menu.GetStatus().title == "b"; }, std::chrono::seconds(1)));
  menu.SetPaused(true);
  CHECK(race.GetStatus().title == "a");
  race.SetPaused(false);
  CHECK(WaitForMusic([&] { return race.GetStatus().title == "b"; }, std::chrono::seconds(1)));
  CHECK_FALSE(rex::audio::HostMusicHasTheMusic());
}

TEST_CASE("Music readiness rejects damaged files and recovers on a new folder", "[audio][host_music][.audio-device]") {
  TempFolder folder;
  fs::create_directories(folder.path / "bad");
  fs::create_directories(folder.path / "good");
  std::ofstream(folder.path / "bad/broken.wav") << "not audio";
  WriteWav(folder.path / "bad/empty.wav", 1, 1, 44100, 16, {});
  WriteWav(folder.path / "good/a.wav", 1, 1, 44100, 16, std::vector<uint8_t>(44100 * 4));
  WriteWav(folder.path / "good/b.wav", 1, 1, 48000, 16, std::vector<uint8_t>(48000 * 4));
  HostMusicPlayer player;
  player.SetVolume(0.0f);
  REQUIRE(player.Play(folder.path / "bad"));
  REQUIRE(WaitForMusic([&] { return !player.GetStatus().playing; }));
  CHECK_FALSE(player.GetStatus().ready);
  REQUIRE(player.Play(folder.path / "good", true));
  REQUIRE(WaitForMusic([&] { return player.GetStatus().ready; }));
  CHECK(player.GetStatus().title == "a");
  CHECK(player.GetStatus().paused);
  player.Stop();
  CHECK_FALSE(player.GetStatus().ready);
}

TEST_CASE("A ready music playlist remains ready while opening its next track", "[audio][host_music][.audio-device]") {
  TempFolder folder;
  WriteWav(folder.path / "a.wav", 1, 1, 48000, 16, std::vector<uint8_t>(48000 / 5 * 2));
  WriteWav(folder.path / "b.wav", 1, 1, 48000, 16, std::vector<uint8_t>(8 * 1024 * 1024));
  HostMusicPlayer player;
  player.SetVolume(0.0f);
  REQUIRE(player.Play(folder.path, true));
  REQUIRE(WaitForMusic([&] { return player.GetStatus().ready; }));
  player.SetPaused(false);
  bool lost_readiness = false;
  const bool advanced = WaitForMusic([&] {
    const auto status = player.GetStatus();
    lost_readiness |= !status.ready;
    return status.title == "b";
  });
  REQUIRE(advanced);
  CHECK_FALSE(lost_readiness);
}

TEST_CASE("A finite music file fades in and out before its silent tail", "[audio][host_music]") {
  TempFolder folder;
  std::vector<float> input(6000, 1.0f);
  std::vector<uint8_t> bytes(input.size() * sizeof(float));
  std::memcpy(bytes.data(), input.data(), bytes.size());
  const auto path = folder.path / "envelope.wav";
  WriteWav(path, 3, 1, 1000, 32, bytes);
  int rate = 0;
  const auto output = HostMusicPlayer::DecodeForTest(path, rate, 2.0f, 0.5f);
  REQUIRE(rate == 1000);
  REQUIRE(output.size() == 13000);
  CHECK(output[0] == 0.0f);
  CHECK(output[2000] > 0.499f);
  CHECK(output[2000] < 0.501f);
  CHECK(output[4000] == 1.0f);
  CHECK(output[7998] == 1.0f);
  CHECK(output[10000] > 0.499f);
  CHECK(output[10000] < 0.501f);
  CHECK(output[11998] == 0.0f);
  CHECK(std::all_of(output.begin() + 12000, output.end(), [](float v) { return v == 0.0f; }));
}

TEST_CASE("A finite music file finishes instead of advancing its own playlist", "[audio][host_music][.audio-device]") {
  TempFolder folder;
  WriteWav(folder.path / "a.wav", 1, 1, 48000, 16, std::vector<uint8_t>(9600));
  WriteWav(folder.path / "b.wav", 1, 1, 48000, 16, std::vector<uint8_t>(96000));
  HostMusicPlayer player;
  player.SetVolume(0.0f);
  REQUIRE(player.PlayFile(folder.path / "a.wav", true, 0.0f, 0.2f));
  REQUIRE(WaitForMusic([&] { return player.GetStatus().ready; }));
  CHECK(player.GetStatus().count == 1);
  player.SetPaused(false);
  REQUIRE(WaitForMusic([&] { return !player.GetStatus().playing; }));
  CHECK_FALSE(player.GetStatus().ready);
  CHECK(player.GetStatus().title == "a");
}

TEST_CASE("Host music plays a WAV whose data chunk overstates its size", "[audio][host_music]") {
  TempFolder folder;
  const auto path = folder.path / "streamed.wav";
  WriteWav(path, 1, 1, 22050, 16, std::vector<uint8_t>(200, 0));
  {
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    file.seekp(40);
    file.write("\xFF\xFF\xFF\xFF", 4);
  }
  int rate = 0;
  const auto samples = HostMusicPlayer::DecodeForTest(path, rate);
  CHECK(rate == 22050);
  CHECK(samples.size() == 200);
}