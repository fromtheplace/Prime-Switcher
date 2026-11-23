#include "PrimeDock.hpp"
#include "../core/Primes.hpp"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include <obs-data.h>

#include <algorithm>
#include <chrono>
#include <random>

namespace {

prime::Ms nowMs()
{
	using namespace std::chrono;
	return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// Spin box that steps and snaps to prime numbers.
class PrimeSpinBox : public QSpinBox {
public:
	explicit PrimeSpinBox(QWidget *parent = nullptr) : QSpinBox(parent)
	{
		setRange(2, 97);
		setSuffix(" s");
		connect(this, &QSpinBox::editingFinished, this,
			[this] { setValue(prime::nearestPrime(value())); });
	}
	void stepBy(int steps) override
	{
		int v = value();
		for (; steps > 0; --steps) v = prime::nextPrime(v);
		for (; steps < 0; ++steps) v = prime::prevPrime(v);
		setValue(v);
	}
};

const int kDefaultPrimes[] = {11, 13, 17, 19, 23, 29, 31, 37};

enum Col { ColOn = 0, ColScene, ColPrime, ColNext, ColCount };

} // namespace

PrimeDock::PrimeDock(prime::ISwitcher *sw, QWidget *parent) : QWidget(parent), sw_(sw)
{
	auto *root = new QVBoxLayout(this);

	table_ = new QTableWidget(0, ColCount, this);
	table_->setHorizontalHeaderLabels({"On", "Scene", "Prime", "Next cut"});
	table_->verticalHeader()->setVisible(false);
	table_->setSelectionMode(QAbstractItemView::NoSelection);
	table_->horizontalHeader()->setSectionResizeMode(ColScene, QHeaderView::Stretch);
	table_->horizontalHeader()->setSectionResizeMode(ColNext, QHeaderView::Stretch);
	root->addWidget(table_);

	auto *opts = new QHBoxLayout;
	opts->addWidget(new QLabel("Min dwell:", this));
	dwell_ = new QDoubleSpinBox(this);
	dwell_->setRange(0.0, 30.0);
	dwell_->setSingleStep(0.5);
	dwell_->setValue(1.5);
	dwell_->setSuffix(" s");
	opts->addWidget(dwell_);
	opts->addStretch();
	root->addLayout(opts);

	button_ = new QPushButton("Start", this);
	connect(button_, &QPushButton::clicked, this, [this] { toggle(); });
	root->addWidget(button_);

	status_ = new QLabel("Stopped", this);
	root->addWidget(status_);

	wake_.setSingleShot(true);
	wake_.setTimerType(Qt::PreciseTimer);
	connect(&wake_, &QTimer::timeout, this, [this] { onWake(); });

	ui_.setInterval(100);
	connect(&ui_, &QTimer::timeout, this, [this] { updateUi(); });
	ui_.start();

	rebuildTable();
}

// ---------------------------------------------------------------- control

void PrimeDock::startSwitching()
{
	if (running()) return;
	collect();

	std::vector<prime::Camera> cams;
	for (const auto &name : rows_) {
		const Cfg &c = cfg_[name];
		cams.push_back({name, static_cast<prime::Ms>(c.period) * 1000, c.enabled});
	}
	sched_.setCameras(cams);
	sched_.setMinDwell(static_cast<prime::Ms>(dwell_->value() * 1000.0));

	sw_->acknowledgeCurrent();
	sched_.start(nowMs(), std::random_device{}(), sw_->currentLive());

	setEditable(false);
	button_->setText("Stop");
	status_->setText("Running");
	arm();
}

void PrimeDock::stopSwitching(const QString &reason)
{
	wake_.stop();
	sched_.stop(); // full reset: phases are discarded
	setEditable(true);
	button_->setText("Start");
	status_->setText(reason.isEmpty() ? QString("Stopped") : QString("Stopped - %1").arg(reason));
	updateUi();
}

void PrimeDock::toggle()
{
	running() ? stopSwitching() : startSwitching();
}

void PrimeDock::onManualSwitch()
{
	if (running()) stopSwitching("manual switch");
}

void PrimeDock::arm()
{
	auto next = sched_.nextWake();
	if (!next) return;
	wake_.start(static_cast<int>(std::max<prime::Ms>(0, *next - nowMs())));
}

void PrimeDock::onWake()
{
	if (!running()) return;
	if (auto id = sched_.tick(nowMs())) sw_->cutTo(*id);
	arm();
}

// ---------------------------------------------------------------- table/UI

void PrimeDock::collect()
{
	for (int r = 0; r < table_->rowCount() && r < static_cast<int>(rows_.size()); ++r) {
		Cfg &c = cfg_[rows_[r]];
		c.enabled = table_->item(r, ColOn)->checkState() == Qt::Checked;
		if (auto *sb = qobject_cast<QSpinBox *>(table_->cellWidget(r, ColPrime)))
			c.period = sb->value();
	}
}

void PrimeDock::refreshScenes()
{
	if (running()) return; // table is locked while running
	collect();
	rebuildTable();
}

void PrimeDock::rebuildTable()
{
	rows_.clear();
	for (const auto &n : sw_->sources()) rows_.push_back(n);

	table_->setRowCount(0);
	table_->setRowCount(static_cast<int>(rows_.size()));

	int idx = 0;
	for (int r = 0; r < static_cast<int>(rows_.size()); ++r) {
		const std::string &name = rows_[r];
		if (!cfg_.count(name)) {
			Cfg c;
			c.period = kDefaultPrimes[idx % 8];
			cfg_[name] = c;
		}
		++idx;
		const Cfg &c = cfg_[name];

		auto *on = new QTableWidgetItem;
		on->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
		on->setCheckState(c.enabled ? Qt::Checked : Qt::Unchecked);
		table_->setItem(r, ColOn, on);

		auto *scene = new QTableWidgetItem(QString::fromStdString(name));
		scene->setFlags(Qt::ItemIsEnabled);
		table_->setItem(r, ColScene, scene);

		auto *sb = new PrimeSpinBox;
		sb->setValue(c.period);
		table_->setCellWidget(r, ColPrime, sb);

		auto *bar = new QProgressBar;
		bar->setTextVisible(true);
		bar->setFormat("-");
		bar->setRange(0, 1);
		bar->setValue(0);
		table_->setCellWidget(r, ColNext, bar);
	}
	table_->resizeColumnToContents(ColOn);
	table_->resizeColumnToContents(ColPrime);
}

void PrimeDock::setEditable(bool on)
{
	for (int r = 0; r < table_->rowCount(); ++r) {
		auto *it = table_->item(r, ColOn);
		it->setFlags(on ? (Qt::ItemIsUserCheckable | Qt::ItemIsEnabled) : Qt::ItemIsEnabled);
		if (auto *w = table_->cellWidget(r, ColPrime)) w->setEnabled(on);
	}
	dwell_->setEnabled(on);
}

void PrimeDock::updateUi()
{
	const std::string live = sw_->currentLive();
	const prime::Ms now = nowMs();

	for (int r = 0; r < table_->rowCount() && r < static_cast<int>(rows_.size()); ++r) {
		const std::string &name = rows_[r];

		auto *scene = table_->item(r, ColScene);
		const bool isLive = (name == live);
		scene->setBackground(isLive ? QBrush(QColor(170, 40, 40)) : QBrush());
		scene->setForeground(isLive ? QBrush(Qt::white) : QBrush());

		auto *bar = qobject_cast<QProgressBar *>(table_->cellWidget(r, ColNext));
		if (!bar) continue;
		const prime::Ms rem = sched_.remaining(name, now);
		if (rem < 0) {
			bar->setRange(0, 1);
			bar->setValue(0);
			bar->setFormat("-");
		} else {
			const int period = cfg_[name].period * 1000;
			bar->setRange(0, period);
			bar->setValue(static_cast<int>(std::min<prime::Ms>(rem, period)));
			bar->setFormat(QString::number(rem / 1000.0, 'f', 1) + " s");
		}
	}
}

// ---------------------------------------------------------------- persistence

void PrimeDock::save(obs_data_t *data)
{
	collect();
	obs_data_t *obj = obs_data_create();
	obs_data_set_double(obj, "min_dwell", dwell_->value());

	obs_data_array_t *arr = obs_data_array_create();
	for (const auto &kv : cfg_) {
		obs_data_t *item = obs_data_create();
		obs_data_set_string(item, "scene", kv.first.c_str());
		obs_data_set_bool(item, "enabled", kv.second.enabled);
		obs_data_set_int(item, "period", kv.second.period);
		obs_data_array_push_back(arr, item);
		obs_data_release(item);
	}
	obs_data_set_array(obj, "scenes", arr);
	obs_data_array_release(arr);

	obs_data_set_obj(data, "prime_switcher", obj);
	obs_data_release(obj);
}

void PrimeDock::load(obs_data_t *data)
{
	obs_data_t *obj = obs_data_get_obj(data, "prime_switcher");
	if (!obj) return;

	double d = obs_data_get_double(obj, "min_dwell");
	if (d > 0.0) dwell_->setValue(d);

	cfg_.clear();
	obs_data_array_t *arr = obs_data_get_array(obj, "scenes");
	if (arr) {
		const size_t n = obs_data_array_count(arr);
		for (size_t i = 0; i < n; ++i) {
			obs_data_t *item = obs_data_array_item(arr, i);
			Cfg c;
			c.enabled = obs_data_get_bool(item, "enabled");
			c.period = prime::nearestPrime(static_cast<int>(obs_data_get_int(item, "period")));
			cfg_[obs_data_get_string(item, "scene")] = c;
			obs_data_release(item);
		}
		obs_data_array_release(arr);
	}
	obs_data_release(obj);

	if (!running()) rebuildTable();
}
