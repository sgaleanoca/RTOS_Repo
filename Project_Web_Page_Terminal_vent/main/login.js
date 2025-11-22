const form = document.getElementById("loginForm");
const errorDiv = document.getElementById("error");

form.addEventListener("submit", async function(e) {
    e.preventDefault();
    errorDiv.textContent = "";
    const formData = new FormData(form);
    const user = formData.get("user");
    const pass = formData.get("pass");
    try {
        const res = await fetch("/login", {
            method: "POST",
            headers: {
                "Content-Type": "application/x-www-form-urlencoded"
            },
            body: "user=" + encodeURIComponent(user) + "&pass=" + encodeURIComponent(pass)
        });
        const text = await res.text();
        if (res.ok) {
            window.location.href = "/";
        } else {
            errorDiv.textContent = text;
        }
    } catch(err) {
        errorDiv.textContent = "Error de conexión con el ESP32.";
    }
});

