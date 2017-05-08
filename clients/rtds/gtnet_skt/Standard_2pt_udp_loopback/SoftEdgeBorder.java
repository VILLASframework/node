

import java.awt.Color;
import java.awt.Component;
import java.awt.Graphics;
import java.awt.Insets;

import javax.swing.border.Border;

	class SoftEdgeBorder implements Border

	{

	  protected int m_w=3;

	  protected int m_h=3;

	  protected Color m_topColor = Color.gray;//white;

	  protected Color m_bottomColor = Color.gray;

	  public SoftEdgeBorder()
	  {

	    m_w=3;

	    m_h=3;

	  }

	  public SoftEdgeBorder(int w, int h) {

	    m_w=w;

	    m_h=h;

	  }

	  public SoftEdgeBorder(int w, int h, Color col)
	  {

	      m_w=w;

	      m_h=h;

	      m_topColor = col;

	      m_bottomColor = col;

	  }

	  public SoftEdgeBorder(int w, int h, Color topColor,

	   Color bottomColor)
	   {

	    m_w=w;

	    m_h=h;

	    m_topColor = topColor;

	    m_bottomColor = bottomColor;

	  }

	  public Insets getBorderInsets(Component c)
	  {

	    return new Insets(m_h, m_w, m_h, m_w);

	  }

	  public  boolean isBorderOpaque() { return true; }

	  public void paintBorder(Component c, Graphics g,

	   int x, int y, int w, int h)
	   {

	    w--;

	    h--;

	    g.setColor(m_topColor);

	    g.drawLine(x, y+h-m_h, x, y+m_h);

	    g.drawArc(x, y, 2*m_w, 2*m_h, 180, -90);

	    g.drawLine(x+m_w, y, x+w-m_w, y);

	    g.drawArc(x+w-2*m_w, y, 2*m_w, 2*m_h, 90, -90);

	    g.setColor(m_bottomColor);

	    g.drawLine(x+w, y+m_h, x+w, y+h-m_h);

	    g.drawArc(x+w-2*m_w, y+h-2*m_h, 2*m_w, 2*m_h, 0, -90);

	    g.drawLine(x+m_w, y+h, x+w-m_w, y+h);

	    g.drawArc(x, y+h-2*m_h, 2*m_w, 2*m_h, -90, -90);

	  }

	}