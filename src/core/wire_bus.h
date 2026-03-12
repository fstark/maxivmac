/*
	wire_bus.h

	Runtime inter-device signal routing, replacing the compile-time
	Wires[] array and #define change notification aliases.

	Part of Phase 4: Device Interface & Machine Object.
*/

#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include <array>

static constexpr int kMaxWires = 64;

class WireBus {
public:
	using ChangeCallback = std::function<void()>;

	void init(int numWires);

	uint8_t  get(int wireId) const        { return wires_[wireId]; }
	void     set(int wireId, uint8_t val);  // sets value + fires callback if changed
	uint8_t* data()                       { return wires_.data(); } // compat with Wires pointer

	// Register a callback for when a wire changes value.
	// Multiple callbacks per wire are supported (appended).
	void onChange(int wireId, ChangeCallback cb);

	// Pulse notification (called once, not on value change)
	void onPulse(int wireId, ChangeCallback cb);
	void pulse(int wireId);

	int numWires() const { return numWires_; }

private:
	int numWires_ = 0;
	std::array<uint8_t, kMaxWires> wires_{};
	std::array<std::vector<ChangeCallback>, kMaxWires> changeCallbacks_{};
	std::array<std::vector<ChangeCallback>, kMaxWires> pulseCallbacks_{};
};

// Global wire bus instance (will move to Machine in a later step)
extern WireBus g_wires;
