//------------------------------------------------------------------------------
// prometheus_gauge.h
//
//
// Description:
// Defines PrometheusGauge class which creates "prometheus::Family<prometheus::Gauge>" and
// "prometheus::Gauge" objects for each kpi with type "GAUGE".
// One object of this class gets created each system health kpi of type "GAUGE", during application startup.
//------------------------------------------------------------------------------

#ifndef _PROMETHEUS_GAUGE_H_
#define _PROMETHEUS_GAUGE_H_

#include <map>
#include <string>

// prometheus
#include <prometheus/family.h>
#include <prometheus/gauge.h>
#include <prometheus/registry.h>

#include "prometheus_kpi.h"

class PrometheusGauge : public PrometheusKpi {
public:
    PrometheusGauge(std::shared_ptr<prometheus::Registry> prometheus_registry, std::string kpi_name,
        std::string kpi_description, std::map<std::string, std::string>& labels);

    PrometheusGauge(const PrometheusGauge&) = delete; // Copy constructor
    PrometheusGauge(PrometheusGauge&&) = delete; // Move constructor
    PrometheusGauge& operator=(const PrometheusGauge&) = delete; // Copy assignment operator
    PrometheusGauge& operator=(PrometheusGauge&&) = delete; // Move assignment operator

    virtual ~PrometheusGauge() = default;

    virtual void SetKpiValue(double kpi_value);
    virtual void SetKpiValue(double kpi_value, std::map<std::string, std::string>& labels);

private:
    PrometheusGauge() = default;

private:
    prometheus::Family<prometheus::Gauge>& m_prometheus_kpi_family; // Gauge Family
    prometheus::Gauge& m_prometheus_kpi; // Gauge
};

#endif
