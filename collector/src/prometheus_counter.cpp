//------------------------------------------------------------------------------
// prometheus_counter.cpp
//------------------------------------------------------------------------------

#include "prometheus_counter.h"

PrometheusCounter::PrometheusCounter(std::shared_ptr<prometheus::Registry> prometheus_registry, std::string kpi_name,
    std::string kpi_description, std::map<std::string, std::string>& labels)
    : m_prometheus_kpi_family(
        prometheus::BuildCounter().Name(kpi_name).Help(kpi_description).Labels(labels).Register(*prometheus_registry))
    , m_prometheus_kpi(m_prometheus_kpi_family.Add({}))

{
}

void PrometheusCounter::SetKpiValue(double kpi_value)
{
    double previous_kpi_value = m_prometheus_kpi.Value();
    if (kpi_value < previous_kpi_value) {
        printf("PrometheusCounter::SetKpiValue,the current KPI value=%ld is less than the previous KPI value=%ld",
            (uint64_t)kpi_value, (uint64_t)previous_kpi_value);
        return;
    } else if (kpi_value > previous_kpi_value) {
        m_prometheus_kpi.Increment(kpi_value - previous_kpi_value);
    }
}

void PrometheusCounter::SetKpiValue(double kpi_value, std::map<std::string, std::string>& labels)
{
    prometheus::Counter& prometheus_kpi = m_prometheus_kpi_family.Add(labels);

    double previous_kpi_value = prometheus_kpi.Value();
    if (kpi_value < previous_kpi_value) {
        printf("PrometheusCounter::SetKpiValue with labels,the current KPI value = %ld is less than the previous KPI "
               "value = %ld",
            (uint64_t)kpi_value, (uint64_t)previous_kpi_value);
        return;
    } else if (kpi_value > previous_kpi_value) {
        prometheus_kpi.Increment(kpi_value - previous_kpi_value);
    }
}
