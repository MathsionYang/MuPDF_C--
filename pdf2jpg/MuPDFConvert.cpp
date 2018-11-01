#include "StdAfx.h"
#include "MuPDFConvert.h"


void fz_free_colorspace_imp(fz_context *ctx, fz_storable *cs_)
{
	fz_colorspace *cs = (fz_colorspace *)cs_;

	if (cs->free_data && cs->data)
		cs->free_data(ctx, cs);
	fz_free(ctx, cs);
}

static void rgb_to_rgb(fz_context *ctx, fz_colorspace *cs, float *rgb, float *xyz)
{
	xyz[0] = rgb[0];
	xyz[1] = rgb[1];
	xyz[2] = rgb[2];
}


const fz_matrix CMuPDFConvert::fz_identity = { 1, 0, 0, 1, 0, 0 };
fz_colorspace CMuPDFConvert::k_device_rgb = { {-1, fz_free_colorspace_imp}, 0, "DeviceRGB", 3, rgb_to_rgb, rgb_to_rgb };

CMuPDFConvert::CMuPDFConvert(void)
{
	fz_device_rgb = &k_device_rgb;

	uselist = 1;
	alphabits = 8;

	//output = NULL;
	resolution = 108;
	rotation = 0;
	res_specified = 0;
	width = 0;
	height = 0;
	fit = 0;
	colorspace = NULL;
	savealpha = 0;
	invert = 0;
	gamma_value = 1;

	m_doc = NULL;
	m_ctx = NULL;
}

CMuPDFConvert::~CMuPDFConvert(void)
{
	if (m_pStream)
	{
		fz_close(m_pStream);
		m_pStream = NULL;
	}

	if (m_doc)
	{
		fz_close_document(m_doc);
		m_doc = NULL;
	}

	if (m_ctx)
	{
		fz_free_context(m_ctx);
		m_ctx = NULL;
	}
	
}


bool CMuPDFConvert::Pdf2Png(const wchar_t* wcharPdfFile/*,const char* imageOutputPath*/,const char* imageName, int &pageNum)
{
	char tempPath[1024];
	//strcpy_s(tempPath, imageOutputPath);
	//strcat_s(tempPath, imageName);
	strcpy_s(tempPath, imageName);
	strcat_s(tempPath, "%d.jpg");

	strcpy_s(output, (strlen(tempPath)+1)*sizeof(char), tempPath);

	m_ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	if (!m_ctx)
	{
#if _DEBUG
		fprintf(stderr, "mupdf cannot initialise context\n");
#endif
		return false;
	}

	fz_try(m_ctx)
	{
		fz_set_aa_level(m_ctx, alphabits);
		colorspace = fz_device_rgb;

		m_pStream = fz_open_file_w(m_ctx, wcharPdfFile);
		if (m_pStream)
		{
			m_doc = fz_open_document_with_stream(m_ctx, ".pdf", m_pStream);
		}

		if (!m_doc)
		{
#if _DEBUG
			fprintf(stderr, "mupdf cannot open pdf\n");
#endif
			return false;
		}

		if (fz_needs_password(m_doc))
		{	
#if _DEBUG
			fprintf(stderr, "mupdf cannot authenticate password\n");
			fz_throw(m_ctx, "mupdf cannot authenticate password: %s", filename);
#endif
			return false;
		}

		int pn = fz_count_pages(m_doc);
		pageNum = pn;
		for (int i=0; i<pn; ++i)
		{
			fz_page *page = fz_load_page(m_doc, i);
			fz_rect bound = fz_bound_page(m_doc, page);

			//PDF-->PNG
			drawpage(m_ctx, m_doc, i+1);

			fz_free_page(m_doc, page);
			page = NULL;
		}	
	}
	fz_catch(m_ctx)
	{
		return false;
	}

	if (m_pStream)
	{
		fz_close(m_pStream);
		m_pStream = NULL;
	}
	if (m_doc)
	{
		fz_close_document(m_doc);
		m_doc = NULL;
	}
	if (m_ctx)
	{
		fz_free_context(m_ctx);
		m_ctx = NULL;
	}
	return true;
}


void CMuPDFConvert::drawpage(fz_context *ctx, fz_document *doc, int pagenum)
{
	fz_page *page;
	fz_display_list *list = NULL;
	fz_device *dev = NULL;
	int start;

	fz_var(list);
	fz_var(dev);

	fz_try(ctx)
	{
		page = fz_load_page(doc, pagenum - 1);
	}
	fz_catch(ctx)
	{
		fz_throw(ctx, "cannot load page %d in file '%s'", pagenum, filename);
	}

	if (uselist)
	{
		fz_try(ctx)
		{
			list = fz_new_display_list(ctx);
			dev = fz_new_list_device(ctx, list);
			fz_run_page(doc, page, dev, fz_identity, NULL);
		}
		fz_catch(ctx)
		{
			fz_free_device(dev);
			fz_free_display_list(ctx, list);
			fz_free_page(doc, page);
			fz_throw(ctx, "cannot draw page %d in file '%s'", pagenum, filename);
		}
		fz_free_device(dev);
		dev = NULL;
	}

	if (output)
	{
		float zoom;
		fz_matrix ctm;
		fz_rect bounds, bounds2;
		fz_bbox bbox;
		fz_pixmap *pix = NULL;
		int w, h;

		fz_var(pix);

		bounds = fz_bound_page(doc, page);
		zoom = resolution / 72;
		ctm = fz_scale(zoom, zoom);
		ctm = fz_concat(ctm, fz_rotate(rotation));
		bounds2 = fz_transform_rect(ctm, bounds);
		bbox = fz_round_rect(bounds2);
		/* Make local copies of our width/height */
		w = width;
		h = height;
		/* If a resolution is specified, check to see whether w/h are
		 * exceeded; if not, unset them. */
		if (res_specified)
		{
			int t;
			t = bbox.x1 - bbox.x0;
			if (w && t <= w)
				w = 0;
			t = bbox.y1 - bbox.y0;
			if (h && t <= h)
				h = 0;
		}
		/* Now w or h will be 0 unless then need to be enforced. */
		if (w || h)
		{
			float scalex = w/(bounds2.x1-bounds2.x0);
			float scaley = h/(bounds2.y1-bounds2.y0);

			if (fit)
			{
				if (w == 0)
					scalex = 1.0f;
				if (h == 0)
					scaley = 1.0f;
			}
			else
			{
				if (w == 0)
					scalex = scaley;
				if (h == 0)
					scaley = scalex;
			}
			if (!fit)
			{
				if (scalex > scaley)
					scalex = scaley;
				else
					scaley = scalex;
			}
			ctm = fz_concat(ctm, fz_scale(scalex, scaley));
			bounds2 = fz_transform_rect(ctm, bounds);
		}
		bbox = fz_round_rect(bounds2);

		/* TODO: banded rendering and multi-page ppm */

		fz_try(ctx)
		{
			pix = fz_new_pixmap_with_bbox(ctx, colorspace, bbox);

			if (savealpha)
				fz_clear_pixmap(ctx, pix);
			else
				fz_clear_pixmap_with_value(ctx, pix, 255);

			dev = fz_new_draw_device(ctx, pix);
			if (list)
				fz_run_display_list(list, dev, ctm, bbox, NULL);
			else
				fz_run_page(doc, page, dev, ctm, NULL);
			fz_free_device(dev);
			dev = NULL;

			if (invert)
				fz_invert_pixmap(ctx, pix);
			if (gamma_value != 1)
				fz_gamma_pixmap(ctx, pix, gamma_value);

			if (savealpha)
				fz_unmultiply_pixmap(ctx, pix);

			if (output)
			{
				char buf[512];
				sprintf(buf, output, pagenum);
				if (strstr(output, ".jpg"))
				{
					fz_write_png(ctx, pix, buf, savealpha);
				}
			}
			fz_drop_pixmap(ctx, pix);
		}
		fz_catch(ctx)
		{
			fz_free_device(dev);
			fz_drop_pixmap(ctx, pix);
			fz_free_display_list(ctx, list);
			fz_free_page(doc, page);
			fz_rethrow(ctx);
		}
	}

	if (list)
		fz_free_display_list(ctx, list);

	fz_free_page(doc, page);

	fz_flush_warnings(ctx);
}