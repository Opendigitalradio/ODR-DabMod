#include <string>
#include <map>
#include <unistd.h>
#include "RemoteControl.h"

using namespace std;

#define BUILD_FOO(p) {   \
  vector<string> p; \
  p.push_back(#p); \
  p.push_back("That's the" #p); \
  parameters_.push_back(p); \
}

class TestControllable : public RemoteControllable
{
    public:
        TestControllable(string name)
        {
            name_ = name;
            
            BUILD_FOO(foo);
            BUILD_FOO(bar);
            BUILD_FOO(baz);

        }

        string get_rc_name() { return name_; };

        list<string> get_supported_parameters() {
            list<string> parameterlist;
            for (list< vector<string> >::iterator it = parameters_.begin(); it != parameters_.end(); it++) {
                parameterlist.push_back((*it)[0]);
            }
            return parameterlist;
        }

        void set_parameter(string parameter, string value) {
            if (parameter == "foo") {
                stringstream ss(value);
                ss >> foo_;
            }
            else if (parameter == "bar") {
                bar_ = value;
            }
            else if (parameter == "baz") {
                stringstream ss(value);
                ss >> baz_;
            }
            else {
                stringstream ss;
                ss << "Parameter '" << parameter << "' is not exported by controllable " << get_rc_name();
                throw ParameterError(ss.str());
            }
        }

        void set_parameter(string parameter, double value) {
            if (parameter == "baz") {
                baz_ = value;
            }
            else {
                stringstream ss;
                ss << "Parameter '" << parameter << "' is not a double in controllable " << get_rc_name();
                throw ParameterError(ss.str());
            }
        }

        void set_parameter(string parameter, long value) {
            if (parameter == "foo") {
                foo_ = value;
            }
            else {
                stringstream ss;
                ss << "Parameter '" << parameter << "' is not a long in controllable " << get_rc_name();
                throw ParameterError(ss.str());
            }
        }

        string get_parameter(string parameter) {
            stringstream ss;
            if (parameter == "foo") {
                ss << foo_;
            }
            else if (parameter == "bar") {
                ss << bar_;
            }
            else if (parameter == "baz") {
                ss << baz_;
            }
            else {
                stringstream ss;
                ss << "Parameter '" << parameter << "' is not exported by controllable " << get_rc_name();
                throw ParameterError(ss.str());
            }
            return ss.str();
        }

        std::list< std::vector<std::string> > get_parameter_descriptions() {
            return parameters_;
        }

    private:
        long foo_;
        std::string bar_;
        std::string name_;
        double baz_;
        std::list< std::vector<std::string> > parameters_;

};

int main()
{
    RemoteControllerTelnet rc (2121);
    TestControllable t("test1");
    TestControllable t2("test2");

    t.enrol_at(rc);
    t2.enrol_at(rc);

    rc.start();

    std::cerr << "Thread has been launched" << std::endl;

    sleep(100);

    std::cerr << "Stop" << std::endl;

    rc.stop();

    return 0;
}

