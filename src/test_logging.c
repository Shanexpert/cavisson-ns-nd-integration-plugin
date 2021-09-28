#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"


int main(int argc, char** argv) {
	VUser user;
	memset(&user, 1, sizeof(user));

	for (i = 0; i < total_writes; i++) {
		log_session_record(1, 1, &user, 1);
	}

	flush_buffers();
}
