#pragma once

#include <ostream>
#include <iomanip>

class DebugPayload
{
public:
	virtual void print_header  (std::ostream& stream) const {};
	virtual void print_payload (std::ostream& stream) const {};
	virtual void print_footer  (std::ostream& stream) const {};
};
