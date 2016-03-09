// Released under the terms of the BSD License
// (C) 2014-2015
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#include <memory>
#include "libsmu.hpp"
#include <iostream>
#include <vector>
#include <cstdint>
#include <string.h>
#include <unistd.h>

using std::cout;
using std::cerr;
using std::endl;
using std::unique_ptr;
using std::vector;
using std::string;

int main(int argc, char **argv) {
	int c;
	const char *file = NULL;

	Session* session = new Session();
	if (session->update_available_devices()) {
		cerr << "error initializing session" << endl;
		return 1;
	}
	for (auto i: session->m_available_devices) {
		session->add_device(&*i);
	}

	session->m_completion_callback = [=](unsigned status){
	};

	session->m_progress_callback = [=](sample_t n) {
	};

	session->m_hotplug_detach_callback = [=](Device* device){
		session->cancel();
		session->remove_device(device);
	};

	session->m_hotplug_attach_callback = [=](Device* device){
		auto dev = session->add_device(device);
		cout << "added_device" << endl;
	};

	if (session->m_devices.size() == 0) {
		cerr << "smu: no supported devices plugged in" << endl;
		return 1;
	}

	while ((c = getopt(argc, argv, "lsc:")) != -1) {
		switch (c) {
			case 'l':
				// list attached device info
				{
					for (auto dev: session->m_devices) {
						printf("%s: serial %s: fw %s: hw %s\n", dev->info()->label, dev->serial(), dev->fwver(), dev->hwver());
					}
				}
				break;
			case 's':
				// stream samples from an attached device to stdout
				{
					auto dev = *(session->m_devices.begin());
					auto dev_info = dev->info();
					for (unsigned ch_i=0; ch_i < dev_info->channel_count; ch_i++) {
						auto ch_info = dev->channel_info(ch_i);
						dev->set_mode(ch_i, 1);
						for (unsigned sig_i=0; sig_i < ch_info->signal_count; sig_i++) {
							auto sig = dev->signal(ch_i, sig_i);
							auto sig_info = sig->info();
							sig->measure_callback([=](float d){cout<<ch_i << "," << sig_i << "," <<d<<endl;});
							if (strncmp(sig_info->label, "Voltage", 4) == 0){
								cout << "setting voltage" << endl;
								sig->source_sine(0, 5, 128, 32);
							}
						}
					}
					session->configure(dev->get_default_rate());
					session->start(0);
					while ( 1 == 1 ) {session->wait_for_completion();};
				}
				break;
			case 'c':
				// write calibration data to a valid, attached m1k device
				{
					file = optarg;

					if (session->m_devices.empty()) {
						cerr << "smu: multiple devices attached, calibration only works on a single device" << endl;
						cerr << "Please detach all devices except the one targeted for calibration." << endl;
						return 1;
					}

					auto dev = *(session->m_devices.begin());
					if (strncmp(dev->info()->label, "ADALM1000", 9) == 0) {
						if (dev->write_calibration(file)) {
							perror("smu: failed to write calibration data");
							return 1;
						}
					} else {
						cerr << "smu: calibration only works with ADALM1000 devices" << endl;
					}
				}
				break;
			default:
				return 1;
		}
	}
	return 0;
}
