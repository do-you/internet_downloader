#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <string>
using namespace std;

struct task_info;

struct parm_implement
{
	const string cmd;
	void(*process)(task_info *, const string &);
	operator const string() const
	{
		return cmd;
	}
};

std::pair<const parm_implement*, const parm_implement*> search_opt(const parm_implement* start, const parm_implement* end, string value);
std::pair<const parm_implement*, const parm_implement*> search_opt(string value);
int result_count(std::pair<const parm_implement*, const parm_implement*> &a);


#endif // PREFERENCES_H
