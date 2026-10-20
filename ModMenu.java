package com.frmod;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;

public final class ModMenu {
    private static final int PURPLE = 0xFF7A1FFF;

    private final Activity act;
    private final boolean overlay;
    private WindowManager wm;
    private WindowManager.LayoutParams lp;
    private LinearLayout panel;
    private LinearLayout body;

    private ModMenu(Activity a, boolean o) {
        act = a;
        overlay = o;
    }

    static void show(final Activity a, final boolean overlay) {
        a.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    new ModMenu(a, overlay).build();
                } catch (Throwable t) {
                    t.printStackTrace();
                }
            }
        });
    }

    private int dp(float v) {
        return (int) (v * act.getResources().getDisplayMetrics().density + 0.5f);
    }

    private static void flag(int id, int v) {
        try {
            Native.setFlag(id, v);
        } catch (Throwable ignored) {
        }
    }

    private static CharSequence rainbow(String s) {
        SpannableString sp = new SpannableString(s);
        for (int i = 0; i < s.length(); i++) {
            int c = Color.HSVToColor(new float[]{i * 360f / s.length(), 1f, 1f});
            sp.setSpan(new ForegroundColorSpan(c), i, i + 1, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        }
        return sp;
    }

    private TextView label(String text) {
        TextView t = new TextView(act);
        t.setText(text);
        t.setTextColor(Color.WHITE);
        t.setTextSize(14);
        t.setPadding(dp(4), dp(8), dp(4), dp(2));
        return t;
    }

    private void addSwitch(LinearLayout parent, String text, final int id) {
        Switch s = new Switch(act);
        s.setText(text);
        s.setTextColor(Color.WHITE);
        s.setTextSize(14);
        s.setPadding(dp(4), dp(6), dp(4), dp(6));
        s.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton b, boolean on) {
                flag(id, on ? 1 : 0);
            }
        });
        parent.addView(s, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
    }

    private Button button(String text, View.OnClickListener l) {
        Button b = new Button(act);
        b.setText(text);
        b.setTextColor(Color.WHITE);
        b.setTextSize(13);
        b.setTypeface(Typeface.DEFAULT_BOLD);
        GradientDrawable g = new GradientDrawable();
        g.setColor(0xFF000000);
        g.setStroke(dp(2), PURPLE);
        g.setCornerRadius(dp(8));
        b.setBackground(g);
        b.setOnClickListener(l);
        return b;
    }

    private void build() {
        panel = new LinearLayout(act);
        panel.setOrientation(LinearLayout.VERTICAL);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(0xF2000000);
        bg.setStroke(dp(2), PURPLE);
        bg.setCornerRadius(dp(8));
        panel.setBackground(bg);
        panel.setPadding(dp(8), dp(6), dp(8), dp(8));

        // ---- titulo (arrastar = mover, tocar = minimizar/expandir)
        TextView title = new TextView(act);
        title.setText(rainbow("FR MOD MENU"));
        title.setTextSize(16);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setGravity(Gravity.CENTER);
        title.setPadding(0, dp(4), 0, dp(6));
        title.setOnTouchListener(new View.OnTouchListener() {
            float offX, offY, startX, startY;
            boolean moved;

            @Override
            public boolean onTouch(View v, MotionEvent e) {
                switch (e.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        startX = e.getRawX();
                        startY = e.getRawY();
                        offX = curX() - startX;
                        offY = curY() - startY;
                        moved = false;
                        return true;
                    case MotionEvent.ACTION_MOVE:
                        if (Math.abs(e.getRawX() - startX) > 12 || Math.abs(e.getRawY() - startY) > 12) {
                            moved = true;
                        }
                        if (moved) moveTo((int) (e.getRawX() + offX), (int) (e.getRawY() + offY));
                        return true;
                    case MotionEvent.ACTION_UP:
                        if (!moved) toggleBody();
                        return true;
                    default:
                        return false;
                }
            }
        });
        panel.addView(title, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        // ---- corpo (rolavel)
        body = new LinearLayout(act);
        body.setOrientation(LinearLayout.VERTICAL);

        ScrollView sv = new ScrollView(act);
        LinearLayout list = new LinearLayout(act);
        list.setOrientation(LinearLayout.VERTICAL);
        sv.addView(list);
        int maxH = (int) (act.getResources().getDisplayMetrics().heightPixels * 0.55f);
        body.addView(sv, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, maxH));

        addSwitch(list, "THICK SMOKE", 0);
        addSwitch(list, "SMOKE DURATION++", 1);
        addSwitch(list, "CAR NO DAMAGE", 2);
        addSwitch(list, "TIRE NO DAMAGE", 3);

        // ---- velocidade
        final TextView speedLabel = label("SPEED: x1.0");
        list.addView(speedLabel);
        SeekBar sb = new SeekBar(act);
        sb.setMax(40);  // x1.0 ate x5.0
        sb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar s, int p, boolean fromUser) {
                speedLabel.setText(String.format("SPEED: x%.1f", 1 + p / 10f));
                flag(4, 100 + p * 10);
            }

            @Override
            public void onStartTrackingTouch(SeekBar s) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar s) {
            }
        });
        list.addView(sb);

        // ---- FPS alvo
        list.addView(label("TARGET FPS"));
        RadioGroup rg = new RadioGroup(act);
        rg.setOrientation(RadioGroup.VERTICAL);
        final int[] vals = {15, 30, 60, 120};
        String[] names = {"15", "30", "60", "120"};
        for (int i = 0; i < vals.length; i++) {
            final int v = vals[i];
            RadioButton rb = new RadioButton(act);
            rb.setText(names[i]);
            rb.setTextColor(Color.WHITE);
            rb.setId(View.generateViewId());
            rb.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                @Override
                public void onCheckedChanged(CompoundButton b, boolean on) {
                    if (on) flag(5, v);
                }
            });
            rg.addView(rb);
        }
        list.addView(rg);

        // ---- botoes
        LinearLayout row = new LinearLayout(act);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setPadding(0, dp(6), 0, 0);
        LinearLayout.LayoutParams bp = new LinearLayout.LayoutParams(0, dp(44), 1f);
        bp.setMargins(dp(3), 0, dp(3), 0);
        row.addView(button("MINIMIZE", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                toggleBody();
            }
        }), bp);
        row.addView(button("FECHAR", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                remove();
            }
        }), bp);
        body.addView(row);

        panel.addView(body);
        attach();
    }

    // ---------------------------------------------------------------- janela
    private void attach() {
        if (overlay) {
            try {
                wm = (WindowManager) act.getApplicationContext().getSystemService(Context.WINDOW_SERVICE);
                int type = Build.VERSION.SDK_INT >= 26
                        ? WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
                        : WindowManager.LayoutParams.TYPE_PHONE;
                lp = new WindowManager.LayoutParams(
                        dp(250), ViewGroup.LayoutParams.WRAP_CONTENT, type,
                        WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                                | WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS,
                        PixelFormat.TRANSLUCENT);
                lp.gravity = Gravity.TOP | Gravity.START;
                lp.x = dp(10);
                lp.y = dp(30);
                wm.addView(panel, lp);
                return;
            } catch (Throwable t) {
                wm = null;  // sem permissao: cai para o modo dentro do jogo
            }
        }
        FrameLayout decor = (FrameLayout) act.getWindow().getDecorView();
        FrameLayout.LayoutParams f = new FrameLayout.LayoutParams(
                dp(250), ViewGroup.LayoutParams.WRAP_CONTENT);
        f.leftMargin = dp(10);
        f.topMargin = dp(30);
        decor.addView(panel, f);
    }

    private float curX() {
        return wm != null ? lp.x : panel.getX();
    }

    private float curY() {
        return wm != null ? lp.y : panel.getY();
    }

    private void moveTo(int x, int y) {
        if (wm != null) {
            lp.x = x;
            lp.y = y;
            wm.updateViewLayout(panel, lp);
        } else {
            panel.setX(x);
            panel.setY(y);
        }
    }

    private void toggleBody() {
        body.setVisibility(body.getVisibility() == View.VISIBLE ? View.GONE : View.VISIBLE);
    }

    private void remove() {
        try {
            if (wm != null) {
                wm.removeView(panel);
            } else if (panel.getParent() instanceof ViewGroup) {
                ((ViewGroup) panel.getParent()).removeView(panel);
            }
        } catch (Throwable ignored) {
        }
    }
}
