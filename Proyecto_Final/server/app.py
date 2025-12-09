from flask import Flask, render_template

app = Flask(__name__)

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/dashboard')
def dashboard():
    return render_template('dashboard.html')

@app.route('/login')
def login():
    return render_template('login.html')

@app.route('/slider')
def slider():
    return render_template('slider.html')

@app.route('/terminal')
def terminal():
    return render_template('terminal.html')

if __name__ == '__main__':
    app.run(debug=True)

