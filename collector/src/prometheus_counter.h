//------------------------------------------------------------------------------
// prometheus_counter.h
//------------------------------------------------------------------------------

#ifndef _PROMETHEUS_COUNTER_H_
#define _PROMETHEUS_COUNTER_H_

#include <map>
#include <string>

// prometheus
#include <prometheus/counter.h>
#include <prometheus/family.h>
#include <prometheus/registry.h>

#include "prometheus_kpi.h"

class PrometheusCounter : public PrometheusKpi {
public:
    PrometheusCounter(std::shared_ptr<prometheus::Registry> prometheus_registry, std::string kpi_name,
        std::string kpi_description, std::map<std::string, std::string>& labels);

    PrometheusCounter(const PrometheusCounter&) = delete; // Copy constructor
    PrometheusCounter(PrometheusCounter&&) = delete; // Move constructor
    PrometheusCounter& operator=(const PrometheusCounter&) = delete; // Copy assignment operator
    PrometheusCounter& operator=(PrometheusCounter&&) = delete; // Move assignment operator

    virtual ~PrometheusCounter() = default;

    virtual void SetKpiValue(double kpi_value);
    virtual void SetKpiValue(double kpi_value, std::map<std::string, std::string>& labels);

private:
    PrometheusCounter() = default;

private:
    prometheus::Family<prometheus::Counter>& m_prometheus_kpi_family; // Counter Family
    prometheus::Counter& m_prometheus_kpi; // Counter
};

#endif
