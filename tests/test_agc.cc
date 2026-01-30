#include <iostream>
#include <vector>
#include <complex>
#include <gnuradio/top_block.h>
#include <gnuradio/blocks/vector_source.h>
#include <gnuradio/blocks/null_sink.h>
#include "../trunk-recorder/gr_blocks/signal_detector_cvf.h"
#include <boost/log/trivial.hpp>

// Mock for Logic Test
struct MockSDR {
    double gain = 0;
    double max_rssi_response = -100; // Simulated RSSI for current gain
    long clipping_response = 0;      // Simulated clipping for current gain

    void set_gain(double g) {
        gain = g;
        // Simulate response:
        // Gain 0-20: Noise floor only (-100)
        // Gain 20-30: Signal appears (-80 to -60)
        // Gain 30-40: Strong signal (-60 to -40)
        // Gain 40-45: Clipping (RSSI -40, clipping > 0)

        if (g < 20) {
            max_rssi_response = -100 + (g/2);
            clipping_response = 0;
        } else if (g < 40) {
            max_rssi_response = -90 + (g * 1.5); // Rises faster
            clipping_response = 0;
        } else {
            max_rssi_response = -30 + (g - 40);
            clipping_response = 100 * (g - 39);
        }
    }
};

void test_gain_logic() {
    std::cout << "[TEST] Testing Gain Calibration Logic..." << std::endl;

    MockSDR sdr;
    double best_gain = 0;
    double best_score = -1e9;

    // Logic replicated from Source::calibrate_gain
    for (double g = 0; g <= 45; g += 5) {
        sdr.set_gain(g);

        long clippings = sdr.clipping_response;
        double max_rssi = sdr.max_rssi_response;
        double noise_floor = -110; // Fixed for simulation

        double dynamic_range = max_rssi - noise_floor;
        double score = dynamic_range;

        if (clippings > 0) {
            score -= 1000;
        }

        std::cout << "  Gain: " << g << " Score: " << score << " (Clip: " << clippings << ", DR: " << dynamic_range << ")" << std::endl;

        if (score > best_score) {
            best_score = score;
            best_gain = g;
        }
    }

    std::cout << "[TEST] Best Gain Selected: " << best_gain << std::endl;

    // Expected: Gain around 35-40 (high DR, no clipping). 40 might clip?
    // At 35: RSSI = -90 + 35*1.5 = -37.5. Clip = 0. DR = 72.5. Score = 72.5.
    // At 40: RSSI = -30 + 0 = -30. Clip = 100 * 1 = 100. Score = 80 - 1000 = -920.
    // So 35 should win.

    if (best_gain == 35) {
        std::cout << "[PASS] Gain Logic selected optimal gain." << std::endl;
    } else {
        std::cout << "[FAIL] Gain Logic selected sub-optimal gain: " << best_gain << std::endl;
        exit(1);
    }
}

void test_clipping_detection() {
    std::cout << "[TEST] Testing Signal Detector Clipping..." << std::endl;

    double rate = 100000;
    int fft_len = 1024;

    gr::top_block_sptr tb = gr::make_top_block("test_agc");

    // Create Data
    std::vector<std::complex<float>> data;
    // 1024 samples of safe data
    for (int i=0; i<1024; i++) data.push_back(std::complex<float>(0.5, 0.5));
    // 1024 samples of clipped data
    for (int i=0; i<1024; i++) data.push_back(std::complex<float>(1.0, 0.0)); // > 0.95 real
    // 1024 samples of clipped data (imag)
    for (int i=0; i<1024; i++) data.push_back(std::complex<float>(0.0, -1.2)); // > 0.95 imag
    // 1024 samples of safe data
    for (int i=0; i<1024; i++) data.push_back(std::complex<float>(0.1, 0.1));

    gr::blocks::vector_source_c::sptr src = gr::blocks::vector_source_c::make(data, false);
    signal_detector_cvf::sptr detector = signal_detector_cvf::make(rate, fft_len);
    // gr::blocks::null_sink::sptr sink = gr::blocks::null_sink::make(sizeof(float) * fft_len); // Detector output is vector float

    // Connect: Vector -> Detector (Detector is a sink/decimator with 0 outputs in this impl)

    tb->connect(src, 0, detector, 0);
    // tb->connect(detector, 0, sink, 0);

    std::cout << "  Running flowgraph..." << std::endl;
    tb->run();

    long count = detector->get_clipping_count();
    std::cout << "  Clipping Count: " << count << std::endl;

    // Expected: 2048 clipped samples.
    if (count == 2048) {
        std::cout << "[PASS] Clipping detection count matches." << std::endl;
    } else {
        std::cout << "[FAIL] Clipping count mismatch. Expected 2048, got " << count << std::endl;
        exit(1);
    }

    detector->reset_clipping_count();
    if (detector->get_clipping_count() == 0) {
        std::cout << "[PASS] Clipping reset works." << std::endl;
    } else {
        std::cout << "[FAIL] Clipping reset failed." << std::endl;
        exit(1);
    }
}

int main() {
    test_gain_logic();
    test_clipping_detection();
    return 0;
}
