#include <QAction>
#include <QMouseEvent>
#include <QMenu>
#include "window-projector.hpp"
#include "display-helpers.hpp"
#include "qt-wrappers.hpp"
#include "platform.hpp"
#include "obs-app.hpp"

OBSProjector::OBSProjector(QWidget *widget, obs_source_t *source_)
	: OBSQTDisplay                 (widget,
	                                Qt::Window | Qt::FramelessWindowHint),
	  source                       (source_),
	  removedSignal                (obs_source_get_signal_handler(source),
	                                "remove", OBSSourceRemoved, this)
{
	setAttribute(Qt::WA_DeleteOnClose, true);

	installEventFilter(CreateShortcutFilter());

	auto addDrawCallback = [this] ()
	{
		obs_display_add_draw_callback(GetDisplay(), OBSRender, this);
		obs_display_set_background_color(GetDisplay(), 0x000000);
	};

	connect(this, &OBSQTDisplay::DisplayCreated, addDrawCallback);

	App()->IncrementSleepInhibition();
}

OBSProjector::~OBSProjector()
{
	if (source)
		obs_source_dec_showing(source);
	App()->DecrementSleepInhibition();
}

void OBSProjector::Init(int monitor)
{
	std::vector<MonitorInfo> monitors;
	GetMonitors(monitors);
	MonitorInfo &mi = monitors[monitor];

	setGeometry(mi.x, mi.y, mi.cx, mi.cy);

	show();

	if (source)
		obs_source_inc_showing(source);

	QAction *action = new QAction(this);
	//action->setShortcut(Qt::Key_Escape);
	addAction(action);

	connect(action, SIGNAL(triggered()), this, SLOT(EscapeTriggered()));
}

void OBSProjector::OBSRender(void *data, uint32_t cx, uint32_t cy)
{
	OBSProjector *window = reinterpret_cast<OBSProjector*>(data);

	uint32_t targetCX;
	uint32_t targetCY;
	int      x, y;
	int      newCX, newCY;
	float    scale;

	if (window->source) {
		targetCX = std::max(obs_source_get_width(window->source), 1u);
		targetCY = std::max(obs_source_get_height(window->source), 1u);
	} else {
		struct obs_video_info ovi;
		obs_get_video_info(&ovi);
		targetCX = ovi.base_width;
		targetCY = ovi.base_height;
	}

	GetScaleAndCenterPos(targetCX, targetCY, cx, cy, x, y, scale);

	newCX = int(scale * float(targetCX));
	newCY = int(scale * float(targetCY));

	gs_viewport_push();
	gs_projection_push();
	gs_ortho(0.0f, float(targetCX), 0.0f, float(targetCY), -100.0f, 100.0f);
	gs_set_viewport(x, y, newCX, newCY);

	if (window->source)
		obs_source_video_render(window->source);
	else
		obs_render_main_view();

	gs_projection_pop();
	gs_viewport_pop();
}

void OBSProjector::OBSSourceRemoved(void *data, calldata_t *params)
{
	OBSProjector *window = reinterpret_cast<OBSProjector*>(data);
	window->deleteLater();

	UNUSED_PARAMETER(params);
}

void OBSProjector::mousePressEvent(QMouseEvent *event)
{
	OBSQTDisplay::mousePressEvent(event);

	if (event->button() == Qt::RightButton) {
		QMenu popup(this);
		popup.addAction(QTStr("Close"), this, SLOT(EscapeTriggered()));
		popup.exec(QCursor::pos());
	}
}

void OBSProjector::EscapeTriggered()
{
	deleteLater();
}
