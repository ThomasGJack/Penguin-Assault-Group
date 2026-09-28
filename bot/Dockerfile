FROM python:3.12-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY bot.py medical.py suivi.py campagne.py trace_image.py ./
COPY suivi_web ./suivi_web
COPY site_web ./site_web
COPY carte ./carte
ENV PYTHONUTF8=1
EXPOSE 8787
CMD ["python", "bot.py"]
