/*  promethus.h
 *
 *  This library aims to enable Metrics-Driven Development for C++ services.
 *  It implements the Prometheus Data Model, a powerful abstraction on which to collect and expose metrics. 
 *  This lets you define and expose internal metrics via an HTTP endpoint on your application’s instance.
 *
 *  See https://jupp0r.github.io/prometheus-cpp for more detailed interface documentation.
 *
 *
*/
#include <prometheus/gauge.h>
#include <prometheus/labels.h>
#include <prometheus/family.h>
#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/detail/future_std.h>

#include "cmonitor.h"
#include <array>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>


class CMonitorPromethues
{
public:
    // singleton instance
    static CMonitorPromethues& instance()
    {
        static CMonitorPromethues sInstance;
        return sInstance;
    }

   void init();
   bool is_prometheus_enabled();
   void add_kpi(const std::string& str, double value, const std::string& s1 ,const std::string& s2);
   void set_expose_port(const std::string& str);
   void set_input_labels(const std::map<std::string, std::string>& labels);

private:
    bool m_bPrometheusEnabled = false;
    std::map<std::string, std::string> m_labels_map;
    std::unique_ptr<prometheus::Exposer> m_exposer;
    std::shared_ptr<prometheus::Registry> m_prometheus_registry = std::make_shared<prometheus::Registry>();
};
