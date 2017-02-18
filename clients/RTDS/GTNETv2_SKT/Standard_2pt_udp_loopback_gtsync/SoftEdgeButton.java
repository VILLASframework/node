

import java.awt.Color;
import java.awt.GradientPaint;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.RenderingHints;
import java.awt.Shape;
import java.awt.geom.RoundRectangle2D;

import javax.swing.Action;
import javax.swing.DefaultButtonModel;
import javax.swing.Icon;
import javax.swing.JButton;

	class SoftEdgeButton extends JButton 
	{
	    private static final float arcwidth  = 6.0f;
	    private static final float archeight = 6.0f;
	    protected static final int focusstroke = 2;
	    protected final Color currFocusBorderCol = new Color(100,113,200, 200);//(110,123,139, 200);//(100,150,255,200); //rgba
	    protected final Color pressedCol = new Color(197, 220, 250);//(230,230,230);
	    protected final Color hoverBorderCol = new Color(140,211,251);//(135,206,250);//Color.ORANGE;
	    protected final Color buttonCol = new Color(202, 225, 255);//(250, 250, 250);
	    protected final Color buttonBorderCol = Color.gray;
	    protected Shape shape;
	    protected Shape border;
	    protected Shape base;

	    public SoftEdgeButton() {
	        this(null, null);
	    }
	    public SoftEdgeButton(Icon icon) {
	        this(null, icon);
	    }
	    public SoftEdgeButton(String text) {
	        this(text, null);
	    }
	    public SoftEdgeButton(Action a) {
	        this();
	        setAction(a);
	    }
	    public SoftEdgeButton(String text, Icon icon) {
	        setModel(new DefaultButtonModel());
	        init(text, icon);
	        setContentAreaFilled(false);
	        setBackground(buttonCol);
	        initShape();
	    }

	    protected void initShape() {
	        if(!getBounds().equals(base)) {
	            base = getBounds();
	            shape = new RoundRectangle2D.Float(0, 0, getWidth()-1, getHeight()-1, arcwidth, archeight);
	            border = new RoundRectangle2D.Float(focusstroke, focusstroke,
	                                                getWidth()-1-focusstroke*2,
	                                                getHeight()-1-focusstroke*2,
	                                                arcwidth, archeight);
	        }
	    }
	    private void paintFocusAndRollover(Graphics2D g2, Color color) {
	        g2.setPaint(new GradientPaint(0, 0, color, getWidth()-1, getHeight()-1, color.brighter(), true));
	        g2.fill(shape);
	        g2.setPaint(new GradientPaint(0, 0, Color.WHITE, 0, (int)(getHeight()-1)/*-1*/, getBackground(), false));
	        //g2.setColor(getBackground());
	        g2.fill(border);
	    }

	    @Override protected void paintComponent(Graphics g) {
	        initShape();
	        Graphics2D g2 = (Graphics2D)g;
	        g2.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
	        if(getModel().isArmed()) {
	            g2.setColor(pressedCol);
	            g2.fill(shape);
	        }else if(isRolloverEnabled() && getModel().isRollover()) {
	            paintFocusAndRollover(g2, hoverBorderCol);
	        }else if(hasFocus()) {
	            paintFocusAndRollover(g2, currFocusBorderCol);
	        }else{

				g2.setPaint(new GradientPaint(0, 0, Color.WHITE, 0, (int)(getHeight()-1)/*-1*/, getBackground(), false));
	           //g2.setColor(getBackground());
	            g2.fill(shape);
	        }
	        g2.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_OFF);
	        g2.setColor(getBackground());
	        super.paintComponent(g2);
	    }
	    @Override protected void paintBorder(Graphics g) {
	        initShape();
	        Graphics2D g2 = (Graphics2D)g;
	        g2.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
	        g2.setColor(buttonBorderCol);
	        g2.draw(shape);
	        g2.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_OFF);
	    }
	    @Override public boolean contains(int x, int y) {
	        initShape();
	        return shape.contains(x, y);
	    }
	}