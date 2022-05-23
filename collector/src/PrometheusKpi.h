//---------------------------------------------------------------------------------------
// PrometheusKpi.h
//
//
// Description:
// Defines PrometheusKpi class which is an abstract base class for PrometheusCounter
// class which creates Counter Family Object for each KPI with type "COUNTER" and
// PrometheusGauge class which creates Gauge Family Object for each KPI with type "GAUGE".
//---------------------------------------------------------------------------------------

#ifndef _PROMETHEUS_KPI_H_
#define _PROMETHEUS_KPI_H_

#include <prometheus/metric_type.h>

typedef prometheus::MetricType KPI_TYPE;

typedef struct {
    std::string kpi_name;
    KPI_TYPE kpi_type;
    std::string description;
} prometheus_kpi_descriptor;

class PrometheusKpi {
public:
    PrometheusKpi() = default;
    PrometheusKpi(const PrometheusKpi&) = delete; // Copy constructor
    PrometheusKpi(PrometheusKpi&&) = delete; // Move constructor
    PrometheusKpi& operator=(const PrometheusKpi&) = delete; // Copy assignment operator
    PrometheusKpi& operator=(PrometheusKpi&&) = delete; // Move assignment operator

    virtual ~PrometheusKpi() = default;

    virtual void SetKpiValue(double kpi_value) = 0;
    virtual void SetKpiValue(double kpi_value, std::map<std::string, std::string>& labels) = 0;
};

#endif
