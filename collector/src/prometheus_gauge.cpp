//------------------------------------------------------------------------------
// prometheus_gauge.cpp
//
// Description:
// Implementation of PrometheusGauge class.
//------------------------------------------------------------------------------

#include "prometheus_gauge.h"

PrometheusGauge::PrometheusGauge(std::shared_ptr<prometheus::Registry> prometheus_registry, std::string kpi_name,
    std::string kpi_description, std::map<std::string, std::string>& labels)
    : m_prometheus_kpi_family(
        prometheus::BuildGauge().Name(kpi_name).Help(kpi_description).Labels(labels).Register(*prometheus_registry))
    , m_prometheus_kpi(m_prometheus_kpi_family.Add({}))

{
}

void PrometheusGauge::SetKpiValue(double kpi_value) { m_prometheus_kpi.Set(kpi_value); }

void PrometheusGauge::SetKpiValue(double kpi_value, std::map<std::string, std::string>& labels)
{
    prometheus::Gauge& prometheus_kpi = m_prometheus_kpi_family.Add(labels);

    prometheus_kpi.Set(kpi_value);
}
