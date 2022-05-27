/*
 * prometheus_gauge.h -- code for collecting prometheus gauge metrics
 * Developer: Satyabrata Bharati.
 * (C) Copyright 2022 Francesco Montorsi
 *
 *  Description:
 Defines PrometheusGauge class which creates and maintains "prometheus::Family<prometheus::Gauge>" and
 "prometheus::Gauge" class reference for each kpi with type "GAUGE".
 One object of PrometheusGauge class gets created per KPI, during application startup.

 prometheus::Family<prometheus::Gauge> :
 This class creates a new metric of type prometheus::Gauge.
 Every metric is uniquely identified by its name and a set of key-value pairs, also known as labels.
 Prometheus's query language allows filtering and aggregation based on metric name and these labels.
 Each new set of labels adds a new dimensional data and is exposed in Prometheus as a time series.
 It is possible to filter the time series with Prometheus's query language by appending a set of labels to match in
 curly braces ({}).

 * prometheus::Gauge :
 This class represents the metric type gauge whose value can arbitrarily go up and down.


 * This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
