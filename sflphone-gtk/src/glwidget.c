#include <glwidget.h>

//! Callback Function to draw the content of the widget
/*!
 * \param widget a pointer to the widget being drawned
 * \param data data on the call back
 * \return the success of the operation
 */
gboolean draw(GtkWidget* widget, gpointer data)
{
	return TRUE;
}

//! Callback Function to reshape the content of the widget
/*!
 * \param widget a pointer to the widget being drawned
 * \param ev a pointer to the event data
 * \param data data on the call back
 * \return the success of the operation
 */
gboolean reshape(GtkWidget* widget, GdkEventConfigure* ev, gpointer data)
{
	return TRUE;
}

//! Callback Function to initialise the content of the widget
/*!
 * \param widget a pointer to the widget being drawned
 * \param data data on the call back
 * \return the success of the operation
 */
gboolean init(GtkWidget* widget, gpointer data)
{
	return TRUE;
}

//! Function to force a redraw of the widget
void redraw(GtkWidget* widget)
{
}

//! Function that creates the opengl widget with all the proper information
/*!
 * \return the created widget
 */
GtkWidget* createGLWidget()
{	return NULL;
}

//! Draws the images from the local capture source
/*!
 * \param widget a pointer to the widget being drawned
 * \param data data on the call back
 * \return the success of the operation
 */
gboolean drawLocal(GtkWidget* widget, gpointer data)
{
	return TRUE;
}


//! Draws the images from the remote source
/*!
 * \param widget a pointer to the widget being drawned
 * \param data data on the call back
 * \return the success of the operation
 */
gboolean drawRemote(GtkWidget* widget, gpointer data)
{
	return TRUE;
}
